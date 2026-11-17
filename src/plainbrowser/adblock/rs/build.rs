// Copyright Plain Browser contributors. MPL-2.0.
fn main() {
    cxx_build::bridge("src/lib.rs").std("c++20").compile("plainbrowser_adblock_bridge");
    println!("cargo:rerun-if-changed=src/lib.rs");
}
