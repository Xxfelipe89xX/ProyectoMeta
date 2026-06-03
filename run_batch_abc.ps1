param(
    [string]$ExePath = ".\evrp",
    [string]$RootInstances = "instances",
    [string]$OutputRoot = "output\results_abc"
)

function Invoke-Set-ABC {
    param(
        [string]$SetName
    )

    $instanceDir = Join-Path $RootInstances $SetName
    if (!(Test-Path $instanceDir)) {
        Write-Host "No existe carpeta:" $instanceDir
        return
    }

    $setOutputDir = Join-Path $OutputRoot $SetName
    New-Item -ItemType Directory -Force -Path $setOutputDir | Out-Null

    $csvPath = Join-Path $setOutputDir "${SetName}_abc_results.csv"

    # CSV Header
    "set,instance,feasible,time_ms,initial_feasible,initial_energy,abc_energy,abc_emissions,abc_routes,visited,total_customers,error" |
    Set-Content -Encoding UTF8 $csvPath

    Get-ChildItem -Path $instanceDir -Filter *.txt | Sort-Object Name | ForEach-Object {
        $instName = $_.Name
        # Run in summary mode
        $line = & $ExePath --summary --abc $_.FullName
        Add-Content -Encoding UTF8 $csvPath $line

        # Parse output for console feedback
        $parts = $line.Split(",")
        $feasible = $parts[2]
        $timeMs = $parts[3]
        $initEnergy = $parts[5]
        $abcEnergy = $parts[6]
        
        # Calculate improvement
        $initDouble = [double]$initEnergy
        $abcDouble = [double]$abcEnergy
        $improvement = 0.0
        if ($initDouble -gt 0) {
            $improvement = [math]::Round(((($initDouble - $abcDouble) / $initDouble) * 100.0), 2)
        }

        Write-Host "[$SetName] Procesada: $instName | Factible: $feasible | Inicial: $initEnergy kWh | ABC: $abcEnergy kWh | Mejora: $improvement% | Tiempo: $timeMs ms"
    }

    $rows = Import-Csv $csvPath
    $feasibleCount = ($rows | Where-Object { $_.feasible -eq "1" }).Count
    $total = $rows.Count

    $summaryPath = Join-Path $setOutputDir "${SetName}_abc_summary.txt"
    @(
        "set=$SetName"
        "instances=$total"
        "feasible_count=$feasibleCount"
        "feasible_pct=$([math]::Round((100.0 * $feasibleCount / $total), 2))"
        "csv=$csvPath"
    ) | Set-Content -Encoding UTF8 $summaryPath

    Write-Host ""
    Write-Host "CSV guardado en: $csvPath"
    Write-Host "Resumen guardado en: $summaryPath"
    Write-Host ""
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

Invoke-Set-ABC -SetName "small"
Invoke-Set-ABC -SetName "large"
