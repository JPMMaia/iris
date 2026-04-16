function Enter-VsDevEnv {
    $vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
    cmd /c "`"$vsPath\Common7\Tools\VsDevCmd.bat`" -arch=x64 -host_arch=x64 && set" |
    ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$') {
            Set-Item -Path "env:$($matches[1])" -Value $matches[2]
        }
    }
}
