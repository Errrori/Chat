<#
.SYNOPSIS
    Build and package a ChatServer deployment bundle.
.DESCRIPTION
    1) Build latest image (or skip with -SkipBuild)
    2) Export image to chat.tar
    3) Bundle chat.tar + compose/config/secret into a zip
.PARAMETER OutputDir
    Output directory under script folder. Default: deploy-output
.PARAMETER SkipBuild
    Skip image build when chat:latest already exists
.EXAMPLE
    .\package-deploy.ps1
    .\package-deploy.ps1 -SkipBuild
#>

param(
    [string]$OutputDir = "deploy-output",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot

Write-Host "=== ChatServer deployment packager ===" -ForegroundColor Cyan

# Step 1: Build image
if (-not $SkipBuild) {
    Write-Host "[1/4] Building image chat:latest ..." -ForegroundColor Yellow
    docker compose build chat
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Image build failed."
        exit 1
    }
}
else {
    Write-Host "[1/4] Skipping build, using existing image." -ForegroundColor DarkGray
}

# Step 2: Prepare output directory
Write-Host "[2/4] Preparing output directory: $OutputDir ..." -ForegroundColor Yellow
$out = Join-Path $ScriptDir $OutputDir
if (Test-Path $out) {
    Remove-Item $out -Recurse -Force
}
New-Item -ItemType Directory -Path $out | Out-Null
New-Item -ItemType Directory -Path (Join-Path $out "uploads") | Out-Null

# Step 3: Export image and copy required files
$tarPath = Join-Path $out "chat.tar"
Write-Host "[3/4] Exporting image to: $tarPath ..." -ForegroundColor Yellow
docker save -o $tarPath chat:latest
if ($LASTEXITCODE -ne 0) {
    Write-Error "Image export failed."
    exit 1
}

Copy-Item (Join-Path $ScriptDir "docker-compose.deploy.yml") (Join-Path $out "docker-compose.yml")
Copy-Item (Join-Path $ScriptDir "config.json") (Join-Path $out "config.json")
Copy-Item (Join-Path $ScriptDir "jwt_secret.json") (Join-Path $out "jwt_secret.json")
Copy-Item (Join-Path $ScriptDir "database.db") (Join-Path $out "database.db")

# Step 4: Create zip package
$version = Get-Date -Format "yyyyMMdd_HHmm"
$zipName = "ChatServer_deploy_$version.zip"
$zipPath = Join-Path $ScriptDir $zipName
Write-Host "[4/4] Creating zip: $zipName ..." -ForegroundColor Yellow
Compress-Archive -Path "$out\*" -DestinationPath $zipPath -Force

Write-Host ""
Write-Host "Done. Package created at:" -ForegroundColor Green
Write-Host "  $zipPath"
Write-Host ""
Write-Host "Receiver quick start:"
Write-Host "  1. Unzip package"
Write-Host "  2. docker load -i chat.tar"
Write-Host "  3. docker compose up -d"
Write-Host "  4. Open http://localhost:10086/static/index.html"
Write-Host ""
Write-Host "Common operations:"
Write-Host "  logs:    docker compose logs -f chat"
Write-Host "  status:  docker compose ps"
Write-Host "  stop:    docker compose down"
Write-Host "  restart: docker compose restart chat"
