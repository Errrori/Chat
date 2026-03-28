# ChatServer 压测一键启动脚本
# 一次启动：HTTP 分布式(master + 2 workers) + WebSocket 分布式(master + 2 workers)
#
# 用法（在任意目录执行）：
#   .\ChatServer\test\load\start_load_test.ps1
#   .\ChatServer\test\load\start_load_test.ps1 -WsWorkers 4
#
# 启动后打开浏览器：
#   HTTP 压测 UI : http://127.0.0.1:8089
#   WS  压测 UI  : http://127.0.0.1:8091

param(
    [string]  $Host_    = "http://127.0.0.1:10086",
    [string]  $RootDir  = (Split-Path (Split-Path (Split-Path $PSScriptRoot))),
    [int]     $HttpWorkers = 2,
    [int]     $WsWorkers   = 2,
    [int]     $HttpPort    = 8089,
    [int]     $WsPort      = 8091
)

$locustFile    = Join-Path $RootDir "ChatServer\test\load\locustfile.py"
$wsLocustFile  = Join-Path $RootDir "ChatServer\test\load\ws_locustfile.py"

# ── 清理旧 Python/Locust 进程 ─────────────────────────────────────
Write-Host "[*] 清理旧的 Locust 进程..." -ForegroundColor Yellow
Get-Process -Name python -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

# ── HTTP 分布式：master ───────────────────────────────────────────
Write-Host "[*] 启动 HTTP master  (UI: http://127.0.0.1:$HttpPort) ..." -ForegroundColor Cyan
Start-Process -FilePath "python" `
    -ArgumentList "-m locust -f `"$locustFile`" --master --host $Host_ --web-port $HttpPort" `
    -WorkingDirectory $RootDir `
    -WindowStyle Minimized
Start-Sleep -Seconds 2

# ── HTTP 分布式：workers ──────────────────────────────────────────
for ($i = 1; $i -le $HttpWorkers; $i++) {
    Write-Host "[*] 启动 HTTP worker #$i ..." -ForegroundColor Cyan
    Start-Process -FilePath "python" `
        -ArgumentList "-m locust -f `"$locustFile`" --worker --master-host 127.0.0.1" `
        -WorkingDirectory $RootDir `
        -WindowStyle Minimized
    Start-Sleep -Milliseconds 500
}

# ── WebSocket 分布式：master ─────────────────────────────────────
Write-Host "[*] 启动 WS master    (UI: http://127.0.0.1:$WsPort)  ..." -ForegroundColor Magenta
Start-Process -FilePath "python" `
    -ArgumentList "-m locust -f `"$wsLocustFile`" --master --host $Host_ --web-port $WsPort --master-bind-port 5558" `
    -WorkingDirectory $RootDir `
    -WindowStyle Minimized
Start-Sleep -Seconds 2

# ── WebSocket 分布式：workers ─────────────────────────────────────
for ($i = 1; $i -le $WsWorkers; $i++) {
    Write-Host "[*] 启动 WS   worker #$i ..." -ForegroundColor Magenta
    Start-Process -FilePath "python" `
        -ArgumentList "-m locust -f `"$wsLocustFile`" --worker --master-host 127.0.0.1 --master-port 5558" `
        -WorkingDirectory $RootDir `
        -WindowStyle Minimized
    Start-Sleep -Milliseconds 500
}

# ── 等待端口就绪后打印汇总 ────────────────────────────────────────
Start-Sleep -Seconds 3
Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host " 全部压测进程已启动" -ForegroundColor Green
Write-Host "  HTTP 压测 UI : http://127.0.0.1:$HttpPort  ($HttpWorkers workers)" -ForegroundColor Green
Write-Host "  WS   压测 UI : http://127.0.0.1:$WsPort   ($WsWorkers  workers)" -ForegroundColor Green
Write-Host ""
Write-Host "  建议参数:" -ForegroundColor Green
Write-Host "    首轮  : 100 users / spawn-rate 20" -ForegroundColor Green
Write-Host "    中等  : 500 users / spawn-rate 50" -ForegroundColor Green
Write-Host "    大规模: 1000 users / spawn-rate 100" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green

# 在默认浏览器打开两个 UI
Start-Process "http://127.0.0.1:$HttpPort"
Start-Process "http://127.0.0.1:$WsPort"
