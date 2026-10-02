//! Inspect every shipping bundle and instantiate each AU without installing it.
use std::{
    ffi::{c_char, c_void, CString},
    path::{Path, PathBuf},
    process::Command,
};

fn output(command: &mut Command) -> String {
    let result = command.output().expect("start validation command");
    assert!(
        result.status.success(),
        "{command:?}: {}",
        String::from_utf8_lossy(&result.stderr)
    );
    String::from_utf8(result.stdout)
        .expect("UTF-8 tool output")
        .trim()
        .to_owned()
}

fn property(plist: &Path, key: &str) -> String {
    output(
        Command::new("/usr/bin/plutil")
            .args(["-extract", key, "raw", "-o", "-"])
            .arg(plist),
    )
}

#[repr(C)]
struct Description {
    kind: u32,
    subtype: u32,
    manufacturer: u32,
    flags: u32,
    mask: u32,
}
type Factory = unsafe extern "C" fn(*const Description) -> *mut c_void;
#[link(name = "AudioToolbox", kind = "framework")]
unsafe extern "C" {
    fn AudioComponentRegister(
        desc: *const Description,
        name: *const c_void,
        version: u32,
        factory: Factory,
    ) -> *mut c_void;
    fn AudioComponentInstanceNew(component: *mut c_void, instance: *mut *mut c_void) -> i32;
    fn AudioComponentInstanceDispose(instance: *mut c_void) -> i32;
    fn AudioUnitInitialize(instance: *mut c_void) -> i32;
    fn AudioUnitUninitialize(instance: *mut c_void) -> i32;
}
#[link(name = "CoreFoundation", kind = "framework")]
unsafe extern "C" {
    fn CFStringCreateWithCString(
        allocator: *const c_void,
        text: *const c_char,
        encoding: u32,
    ) -> *const c_void;
    fn CFRelease(object: *const c_void);
}
unsafe extern "C" {
    fn dlopen(path: *const c_char, flags: i32) -> *mut c_void;
    fn dlsym(handle: *mut c_void, symbol: *const c_char) -> *mut c_void;
}
fn fourcc(value: &str) -> u32 {
    u32::from_be_bytes(value.as_bytes().try_into().expect("four-character AU code"))
}

fn au_smoke(binary: &Path, plist: &Path) {
    let description = Description {
        kind: fourcc(&property(plist, "AudioComponents.0.type")),
        subtype: fourcc(&property(plist, "AudioComponents.0.subtype")),
        manufacturer: fourcc(&property(plist, "AudioComponents.0.manufacturer")),
        flags: 0,
        mask: 0,
    };
    let path = CString::new(binary.as_os_str().as_encoded_bytes()).unwrap();
    let symbol = CString::new(property(plist, "AudioComponents.0.factoryFunction")).unwrap();
    let name = CString::new(property(plist, "AudioComponents.0.name")).unwrap();
    let version = property(plist, "AudioComponents.0.version")
        .parse()
        .unwrap();
    // Apple owns the registration for this process; keep the library loaded
    // until exit. Never register with the system or change plugin directories.
    unsafe {
        let library = dlopen(path.as_ptr(), 2 | 4); // RTLD_NOW | RTLD_LOCAL
        assert!(!library.is_null(), "AU library loads");
        let factory = dlsym(library, symbol.as_ptr());
        assert!(!factory.is_null(), "AU factory is exported");
        let title = CFStringCreateWithCString(std::ptr::null(), name.as_ptr(), 0x08000100);
        assert!(!title.is_null());
        let component = AudioComponentRegister(
            &description,
            title,
            version,
            std::mem::transmute::<*mut c_void, Factory>(factory),
        );
        CFRelease(title);
        assert!(!component.is_null(), "AU registers in-process");
        let mut instance = std::ptr::null_mut();
        assert_eq!(
            AudioComponentInstanceNew(component, &mut instance),
            0,
            "AU instantiates"
        );
        assert_eq!(AudioUnitInitialize(instance), 0, "AU initializes");
        assert_eq!(AudioUnitUninitialize(instance), 0, "AU uninitializes");
        assert_eq!(AudioComponentInstanceDispose(instance), 0, "AU disposes");
    }
}

fn main() {
    let root = PathBuf::from(std::env::args_os().nth(1).expect("build directory"));
    let version = std::env::args().nth(2).expect("expected project version");
    let parts: Vec<u32> = version
        .split('.')
        .map(|part| part.parse().expect("numeric version"))
        .collect();
    assert_eq!(parts.len(), 3, "major.minor.patch version");
    assert!(
        parts[0] <= 65535 && parts[1] <= 255 && parts[2] <= 255,
        "AU version fields fit"
    );
    let au_version = (parts[0] << 16) | (parts[1] << 8) | parts[2];
    let package = std::env::args_os().nth(3).map(PathBuf::from);
    let expanded = root.join(format!("package-inspection-{}", std::process::id()));
    if let Some(package) = &package {
        assert!(!expanded.exists(), "inspection destination must be new");
        output(
            Command::new("/usr/sbin/pkgutil")
                .arg("--expand-full")
                .arg(package)
                .arg(&expanded),
        );
        let distribution = std::fs::read_to_string(expanded.join("Distribution")).unwrap();
        let packages: Vec<_> = distribution
            .lines()
            .filter(|line| line.contains("<pkg-ref ") && line.contains(" version="))
            .collect();
        assert_eq!(packages.len(), 13, "twelve plugins and notices");
        for package in packages {
            assert!(
                package.contains(&format!("version=\"{version}\"")),
                "installer component version matches project"
            );
        }
        assert!(distribution.contains("enable_localSystem=\"true\""));
        assert!(distribution.contains("<allowed-os-versions>"));
        assert!(
            distribution.contains("hostArchitectures=\"arm64,x86_64\""),
            "installer supports native Apple Silicon without Rosetta"
        );
        assert!(
            !distribution.contains("<script>"),
            "installer needs no scripts"
        );
    }
    for (target, name) in [
        ("DensityD01", "Density D-01"),
        ("HarmonicH01", "Harmonic H-01"),
        ("SequenceS01", "Sequence S-01"),
        ("LoopL01", "Loop L-01"),
        ("ImpulseI01", "Impulse I-01"),
        ("FieldF01", "Field F-01"),
    ] {
        for (format, extension) in [("AU", "component"), ("VST3", "vst3")] {
            let mut bundle = root.join(format!(
                "{target}_artefacts/Release/{format}/{name}.{extension}"
            ));
            if package.is_some() {
                let mut matches = Vec::new();
                find_bundle(&expanded, &format!("{name}.{extension}"), &mut matches);
                assert_eq!(
                    matches.len(),
                    1,
                    "exactly one packaged copy of {name} {format}"
                );
                bundle = matches.pop().unwrap();
                let folder = if format == "AU" { "Components" } else { "VST3" };
                assert!(
                    bundle.ends_with(format!(
                        "Library/Audio/Plug-Ins/{folder}/{name}.{extension}"
                    )),
                    "correct installation path"
                );
            }
            let binary = bundle.join("Contents/MacOS").join(name);
            let plist = bundle.join("Contents/Info.plist");
            assert_eq!(
                property(&plist, "CFBundleShortVersionString"),
                version,
                "{name} {format} display version"
            );
            assert_eq!(
                property(&plist, "CFBundleVersion"),
                version,
                "{name} {format} build version"
            );
            if format == "AU" {
                assert_eq!(
                    property(&plist, "AudioComponents.0.version"),
                    au_version.to_string(),
                    "{name} AU numeric version"
                );
            }
            let architectures = output(Command::new("/usr/bin/lipo").arg("-archs").arg(&binary));
            let mut architectures: Vec<_> = architectures.split_whitespace().collect();
            architectures.sort_unstable();
            assert_eq!(architectures, ["arm64", "x86_64"], "exact universal slices");
            output(
                Command::new("/usr/bin/codesign")
                    .args(["--verify", "--strict", "--deep"])
                    .arg(&bundle),
            );
            let links = output(Command::new("/usr/bin/otool").arg("-L").arg(&binary));
            for dependency in links.lines().filter(|line| line.starts_with('\t')) {
                assert!(
                    dependency.starts_with("\t/usr/lib/")
                        || dependency.starts_with("\t/System/Library/"),
                    "Non-system runtime dependency: {dependency}"
                );
            }
            if format == "AU" {
                au_smoke(&binary, &bundle.join("Contents/Info.plist"));
            }
            println!(
                "{name} {format}: universal, signed, self-contained{}",
                if format == "AU" { ", instantiated" } else { "" }
            );
        }
    }
    let mut cleanup = root.join("cleanup/Aste Plugin Cleanup.app");
    if package.is_some() {
        let mut matches = Vec::new();
        find_bundle(&expanded, "Aste Plugin Cleanup.app", &mut matches);
        assert_eq!(matches.len(), 1, "exactly one installed cleanup utility");
        cleanup = matches.pop().unwrap();
        assert!(cleanup.ends_with("Applications/Aste Plugin Cleanup.app"));
    }
    assert_eq!(
        property(
            &cleanup.join("Contents/Info.plist"),
            "CFBundleShortVersionString"
        ),
        version
    );
    output(
        Command::new("/usr/bin/codesign")
            .args(["--verify", "--strict", "--deep"])
            .arg(&cleanup),
    );
    let cleanup_binary = cleanup.join("Contents/MacOS/aste-plugin-cleanup");
    let architectures = output(
        Command::new("/usr/bin/lipo")
            .arg("-archs")
            .arg(&cleanup_binary),
    );
    let mut architectures: Vec<_> = architectures.split_whitespace().collect();
    architectures.sort_unstable();
    assert_eq!(architectures, ["arm64", "x86_64"]);
    output(Command::new(cleanup_binary).arg("--help"));
    println!("Cleanup utility: universal, signed, version matched, installed path checked");
    if package.is_some() {
        std::fs::remove_dir_all(expanded).expect("remove inspection output");
    }
}

fn find_bundle(directory: &Path, name: &str, matches: &mut Vec<PathBuf>) {
    for entry in std::fs::read_dir(directory).unwrap() {
        let entry = entry.unwrap();
        if entry.file_type().unwrap().is_dir() {
            if entry.file_name() == name {
                matches.push(entry.path());
            } else {
                find_bundle(&entry.path(), name, matches);
            }
        }
    }
}
