"""Build the Android APK with installed SDK command line tools and a local test key."""

import argparse
from pathlib import Path
import os
import subprocess
from tempfile import TemporaryDirectory
from zipfile import ZipFile


def run(*command):
    subprocess.run([str(item) for item in command], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--jdk", type=Path, required=True)
    parser.add_argument("--build-tools", default="36.0.0")
    args = parser.parse_args()
    source = Path(__file__).resolve().parent
    build = source.parent / "build/android-clock"
    build.mkdir(parents=True, exist_ok=True)
    tools = args.sdk / "build-tools" / args.build_tools
    platform = args.sdk / "platforms/android-34/android.jar"
    exe = ".exe" if os.name == "nt" else ""
    java = args.jdk / ("bin/java" + exe)
    javac = args.jdk / ("bin/javac" + exe)
    keytool = args.jdk / ("bin/keytool" + exe)
    aapt2 = tools / ("aapt2" + exe)
    align = tools / ("zipalign" + exe)
    apksigner = tools / "lib/apksigner.jar"
    key = build / "local-test.keystore"

    with TemporaryDirectory(prefix="apk-", dir=build) as directory:
        work = Path(directory)
        classes = work / "classes"
        test_classes = work / "test-classes"
        generated = work / "generated"
        classes.mkdir()
        test_classes.mkdir()
        generated.mkdir()
        run(javac, "-encoding", "UTF-8", "--release", "8", "-d", test_classes,
            source / "src/cn/bupt/shaheclock/ClockProtocol.java",
            source / "test/cn/bupt/shaheclock/ClockProtocolTest.java")
        run(java, "-cp", test_classes, "cn.bupt.shaheclock.ClockProtocolTest")
        compiled = work / "resources.zip"
        unsigned = work / "unsigned.apk"
        aligned = work / "aligned.apk"
        run(aapt2, "compile", "--dir", source / "res", "-o", compiled)
        run(aapt2, "link", "-I", platform, "--manifest", source / "AndroidManifest.xml",
            "--java", generated, "-o", unsigned, compiled)
        run(javac, "-encoding", "UTF-8", "--release", "8", "-cp", platform, "-d", classes,
            *sorted((source / "src").rglob("*.java")), *sorted(generated.rglob("*.java")))
        run(java, "-cp", tools / "lib/d8.jar", "com.android.tools.r8.D8",
            "--lib", platform, "--min-api", "26", "--output", work, *sorted(classes.rglob("*.class")))
        with ZipFile(unsigned, "a") as archive:
            archive.write(work / "classes.dex", "classes.dex")
        run(align, "-f", "4", unsigned, aligned)
        if not key.exists():
            run(keytool, "-genkeypair", "-keystore", key, "-storepass", "android",
                "-keypass", "android", "-alias", "shahe-clock-test", "-keyalg", "RSA",
                "-keysize", "2048", "-validity", "10000", "-dname", "CN=Shahe Clock Local Test")
        signed = work / "shahe-clock.apk"
        run(java, "-jar", apksigner, "sign", "--ks", key, "--ks-pass", "pass:android",
            "--out", signed, aligned)
        run(align, "-c", "4", signed)
        run(java, "-jar", apksigner, "verify", "--verbose", signed)
        run(aapt2, "dump", "badging", signed)
        output = build / "shahe-clock.apk"
        signed.replace(output)
    print(output)


if __name__ == "__main__":
    main()
