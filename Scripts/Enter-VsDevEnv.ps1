function Enter-VsDevEnv {
    $originalVcpkgRoot = $env:VCPKG_ROOT
    $vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -all -products * -latest -property installationPath
    cmd /c "`"$vsPath\Common7\Tools\VsDevCmd.bat`" -arch=x64 -host_arch=x64 `-vcvars_ver=14.42.34433 && set" |
    ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$') {
            Set-Item -Path "env:$($matches[1])" -Value $matches[2]
        }
    }

    if ($null -ne $originalVcpkgRoot) {
        Set-Item -Path "env:VCPKG_ROOT" -Value $originalVcpkgRoot
    } else {
        Remove-Item -Path "env:VCPKG_ROOT" -ErrorAction SilentlyContinue
    }
}
