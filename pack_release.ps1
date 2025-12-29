param(
    [string]$OutDir = "Dist",
    [string]$BuildDir = "Build",
    [string]$Config = "Release"
)

Write-Host "Building project ($Config)..."
cmake --build "$PSScriptRoot\$BuildDir" --config $Config -j

$staging = Join-Path $PSScriptRoot "$OutDir\$Config"
if(Test-Path $staging){ Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Path $staging | Out-Null

Write-Host "Installing to staging: $staging"
cmake --install "$PSScriptRoot\$BuildDir" --config $Config --prefix "$staging"

# 搜索并复制任何依赖的 DLL（如果存在）到 staging 根目录
# 1) 复制 build/_deps 中的第三方 DLL（如果有）
$deps = Get-ChildItem -Path "$PSScriptRoot\$BuildDir\_deps" -Recurse -Filter *.dll -ErrorAction SilentlyContinue
if($deps){
    Write-Host "Found DLLs in _deps, copying to staging root..."
    foreach($d in $deps){ Copy-Item $d.FullName -Destination $staging -Force }
}

# 2) 复制构建输出目录（例如 */Release）中的 DLL 到 staging
Write-Host "Searching for build output 'Release' directories to collect DLLs..."
$releaseDirs = Get-ChildItem -Path "$PSScriptRoot\$BuildDir" -Recurse -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -eq 'Release' }
if($releaseDirs){
    foreach($dir in $releaseDirs){
        $dlls = Get-ChildItem -Path $dir.FullName -Filter *.dll -File -ErrorAction SilentlyContinue
        if($dlls){
            Write-Host "Copying DLLs from: $($dir.FullName)"
            foreach($dll in $dlls){
                # 排除明显的系统运行时 DLL（可按需扩展）
                if($dll.Name -notmatch '^(msvcp|vcruntime|api-ms|ucrtbase|KERNEL32|ntdll|concrt|msvcr)'){
                    Copy-Item $dll.FullName -Destination $staging -Force
                }
            }
        }
    }
}

# 3) 额外扫描 bin/*/Release 或 x64/Release 等常见位置
$commonPaths = @()
$commonPaths += [System.IO.Path]::Combine($PSScriptRoot, $BuildDir, 'bin', $Config)
$commonPaths += [System.IO.Path]::Combine($PSScriptRoot, $BuildDir, 'x64', $Config)
$commonPaths += [System.IO.Path]::Combine($PSScriptRoot, $BuildDir, $Config)
foreach($cp in $commonPaths){
    if(Test-Path $cp){
        $dlls = Get-ChildItem -Path $cp -Filter *.dll -File -ErrorAction SilentlyContinue
        foreach($dll in $dlls){
            if($dll.Name -notmatch '^(msvcp|vcruntime|api-ms|ucrtbase|KERNEL32|ntdll|concrt|msvcr)'){
                Copy-Item $dll.FullName -Destination $staging -Force
            }
        }
    }
}

# 复制 icon 文件以便用户参考（exe 已通过资源嵌入图标）
$iconSrc = Join-Path $PSScriptRoot "resources\windows\icon.ico"
if(Test-Path $iconSrc){ Copy-Item $iconSrc -Destination $staging -Force }

Write-Host "Creating ZIP package..."
$zipPath = Join-Path $PSScriptRoot "$OutDir\${env:COMPUTERNAME}_TankMaze_$Config.zip"
if(Test-Path $zipPath){ Remove-Item $zipPath -Force }
if(Test-Path "$PSScriptRoot\$OutDir\$Config"){
    Compress-Archive -Path "$PSScriptRoot\$OutDir\$Config\*" -DestinationPath $zipPath -Force
    Write-Host "Package created: $zipPath"
} else {
    Write-Error "Staging folder not found: $staging"
}

Write-Host "Done. Distribution folder: $staging"
