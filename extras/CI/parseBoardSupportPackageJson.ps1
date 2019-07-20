[uri[]]$bspUris =
    #"https://adafruit.github.io/arduino-board-index/package_adafruit_index.json",
    #"http://arduino.esp8266.com/stable/package_esp8266com_index.json",
    #"https://dl.espressif.com/dl/package_esp32_index.json",
    "http://dan.drown.org/stm32duino/package_STM32duino_index.json";

[System.Collections.Generic.List[System.Object]]$highestVersionsOnly = [System.Collections.Generic.List[System.Object]]::new();

$bspUris | ForEach-Object {
    [uri]$uri = $_;
    $webResponse = Invoke-WebRequest $uri;
    $json = $webResponse.Content | ConvertFrom-Json;

    [string[]]$architectures = $json.packages.platforms.architecture | Sort-Object -Unique;

    $architectures | ForEach-Object {
        [String]$arch = $_;
        
        [System.Version]$maxVersion = (
            $json.packages.platforms | Where-Object {
                $_.architecture -eq $arch
            } | ForEach-Object {
                [System.Version]::new($_.version)
            } | Sort-Object -Descending
            )[0];
        $latestPlatform = (
            $json.packages.platforms | Where-Object {
                $_.architecture -eq $arch
            } | Where-Object {
                [System.Version]::new($_.version) -eq $maxVersion
            } | Sort-Object -Descending
            )[0];
        $highestVersionsOnly.Add($latestPlatform);

        [String]::Format("{0,10} {1,10} {2}", $arch, $latestPlatform.version, $latestPlatform.url);
    }
}




