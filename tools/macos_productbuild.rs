//! CPack does not expose Installer's hostArchitectures option. Keep Apple's
//! productbuild and its arguments, adding only the universal declaration.
use std::{
    ffi::OsStr,
    fs,
    process::{exit, Command},
};

fn universal_distribution(xml: &str) -> String {
    assert_eq!(
        xml.matches("<options ").count(),
        1,
        "one installer options element"
    );
    assert!(
        !xml.contains("hostArchitectures="),
        "CPack must not supply a conflicting architecture declaration"
    );
    xml.replacen(
        "<options ",
        "<options hostArchitectures=\"arm64,x86_64\" ",
        1,
    )
}

fn main() {
    let args: Vec<_> = std::env::args_os().skip(1).collect();
    if let Some(index) = args
        .iter()
        .position(|arg| arg == OsStr::new("--distribution"))
    {
        let path = args.get(index + 1).expect("distribution path");
        let xml = fs::read_to_string(path).expect("read generated distribution");
        fs::write(path, universal_distribution(&xml)).expect("declare universal installer");
    }
    let status = Command::new("/usr/bin/productbuild")
        .args(args)
        .status()
        .expect("run Apple productbuild");
    exit(status.code().unwrap_or(1));
}

#[test]
fn preserves_choices_and_other_options() {
    let before = "<options customize=\"allow\"/><choice id=\"AU\"/>";
    assert_eq!(
        universal_distribution(before),
        "<options hostArchitectures=\"arm64,x86_64\" customize=\"allow\"/><choice id=\"AU\"/>"
    );
    assert!(std::panic::catch_unwind(|| universal_distribution(
        "<options hostArchitectures=\"x86_64\"/>"
    ))
    .is_err());
}
