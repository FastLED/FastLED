import os

def main():
    platforms = ["uno", "esp32dev", "esp32s3", "esp32c3", "esp32p4", "teensy41"]

    for platform in platforms:
        command = f"bash compile {platform} Blink --docker --build"
        print(f"\n=== Running: {command} ===\n")
        result = os.system(command)

        if result != 0:
            print(f"❌ Failed on platform: {platform} (exit code {result})")
        else:
            print(f"✅ Success on platform: {platform}")

if __name__ == "__main__":
    main()
