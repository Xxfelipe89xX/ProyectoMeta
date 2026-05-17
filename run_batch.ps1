param(
    [string]$ExePath = ".\evrp",
    [string]$RootInstances = "instances",
    [string]$OutputRoot = "output\results"
)

function Invoke-Set {
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

    $csvPath = Join-Path $setOutputDir "${SetName}_results.csv"

    "set,instance,feasible,total_ms,read_ms,greedy_ms,repair_ms,local_search_ms,energy_kwh,emissions_kg,routes,visited_customers,total_customers,error" |
    Set-Content -Encoding UTF8 $csvPath

    Get-ChildItem -Path $instanceDir -Filter *.txt | Sort-Object Name | ForEach-Object {
        $result = & $ExePath --summary $_.FullName
        $result | Add-Content -Encoding UTF8 $csvPath
        Write-Host "[$SetName] Procesada:" $_.Name
    }

    $rows = Import-Csv $csvPath
    $feasibleRows = $rows | Where-Object { $_.feasible -eq "1" }

    $total = $rows.Count
    $feasible = $feasibleRows.Count
    $infeasible = $total - $feasible

    $avgTotalMs = if ($total -gt 0) {
        [math]::Round((($rows | Measure-Object -Property total_ms -Average).Average), 2)
    }
    else { 0 }

    $avgEnergyFeasible = if ($feasible -gt 0) {
        [math]::Round((($feasibleRows | Measure-Object -Property energy_kwh -Average).Average), 4)
    }
    else { 0 }

    $avgRoutesFeasible = if ($feasible -gt 0) {
        [math]::Round((($feasibleRows | Measure-Object -Property routes -Average).Average), 2)
    }
    else { 0 }

    $feasiblePct = if ($total -gt 0) {
        [math]::Round((100.0 * $feasible / $total), 2)
    }
    else { 0 }

    $summaryPath = Join-Path $setOutputDir "${SetName}_summary.txt"
    @(
        "set=$SetName"
        "instances=$total"
        "feasible=$feasible"
        "infeasible=$infeasible"
        "feasible_pct=$feasiblePct"
        "avg_total_ms=$avgTotalMs"
        "avg_energy_kwh_feasible=$avgEnergyFeasible"
        "avg_routes_feasible=$avgRoutesFeasible"
        "csv=$csvPath"
    ) | Set-Content -Encoding UTF8 $summaryPath

    Write-Host ""
    Write-Host "CSV guardado en: $csvPath"
    Write-Host "Resumen guardado en: $summaryPath"
    Write-Host ""
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

Invoke-Set -SetName "small"
Invoke-Set -SetName "large"