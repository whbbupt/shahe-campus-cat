param(
    [string]$Port = "COM5",
    [int]$Baud = 1000000,
    [int]$Seconds = 15,
    [string]$Out = "",
    [string]$Send = ""
)

# 串口记录器：默认只读；加 -Send "reboot" 时只发送这一条命令，不改固件。
# 用法：powershell -ExecutionPolicy Bypass -File scripts/capture_serial.ps1 -Port COM5 -Seconds 20
# 运行后按板子复位键，脚本会保存原始启动日志到 evidence/pet/。

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Out)) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $Out = "evidence/pet/serial_v0180_$stamp.txt"
}
$dir = Split-Path -Parent $Out
if ($dir -and -not (Test-Path $dir)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

$sp = New-Object System.IO.Ports.SerialPort
$sp.PortName = $Port
$sp.BaudRate = $Baud
$sp.Parity = [System.IO.Ports.Parity]::None
$sp.DataBits = 8
$sp.StopBits = [System.IO.Ports.StopBits]::One
$sp.Handshake = [System.IO.Ports.Handshake]::None
$sp.ReadTimeout = 200
$sp.WriteTimeout = 200

try {
    $sp.Open()
} catch {
    Write-Error "无法打开 $Port（$($_.Exception.Message)）。先关闭 CodeKit/串口助手，确认端口未被占用。"
    exit 1
}

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("# port=$Port baud=$Baud started=$(Get-Date -Format o)")
[void]$sb.AppendLine("# 默认只读采集；如指定 -Send，只发送一次该命令。")
if ($Send.Length -gt 0) {
    Start-Sleep -Milliseconds 500
    [void]$sb.AppendLine("# sent: $Send")
    $sp.Write($Send + "`r`n")
}
$watch = [System.Diagnostics.Stopwatch]::StartNew()
try {
    while ($watch.Elapsed.TotalSeconds -lt $Seconds) {
        try {
            $chunk = $sp.ReadExisting()
            if ($chunk.Length -gt 0) {
                [void]$sb.Append($chunk)
                Write-Host -NoNewline $chunk
            }
        } catch [System.TimeoutException] {
        }
        Start-Sleep -Milliseconds 50
    }
} finally {
    $sp.Close()
    [void]$sb.AppendLine()
    [void]$sb.AppendLine("# stopped=$(Get-Date -Format o)")
    $full = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path (Get-Location).Path $Out }
    [System.IO.File]::WriteAllText($full, $sb.ToString(), (New-Object System.Text.UTF8Encoding($false)))
}

Write-Host ""
Write-Host "已保存原始串口日志：$Out"
Write-Host "请把该文件和屏幕照片一起放进 evidence/pet/，再更新 manifest 的 physical_verification。"
