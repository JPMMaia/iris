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

# Call the function
Enter-VsDevEnv

# Output environment variables for capture (only when --output-env flag is passed)
if ($args -contains "--output-env") {
    $envVarsToCapture = @("VCINSTALLDIR", "VCToolsVersion", "VisualStudioVersion",
                           "PATH", "INCLUDE", "LIB", "LIBPATH", "VCPKG_ROOT", "UCRTVersion", "WindowsSDKLibVersion", "WindowsSDKVersion", "WindowsSdkDir", "WindowsSDKLibDir", "WindowsSDKIncludeDir")

    foreach ($var in $envVarsToCapture) {
        $val = Get-Item -Path "env:$var" -ErrorAction SilentlyContinue
        if ($val) {
            Write-Output "$var=$($val.Value)"
        }
    }
}
