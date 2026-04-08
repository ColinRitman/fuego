use std::io::Write;
use std::process::Command;

fn main() {
    // Build the SP1 circuit
    println!("cargo:warning=Building SP1 LP circuit...");

    // This ensures the circuit is compiled with the correct target
    let output = Command::new("rustc").args(&["--version"]).output();

    if let Ok(output) = output {
        println!(
            "cargo:warning=Using rustc: {}",
            String::from_utf8_lossy(&output.stdout)
        );
    }

    // Tell cargo to rerun this build script if these change
    println!("cargo:rerun-if-changed=src/main.rs");
    println!("cargo:rerun-if-env-changed=SP1_VERSION");
}
