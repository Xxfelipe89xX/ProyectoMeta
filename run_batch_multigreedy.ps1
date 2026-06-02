param(
    [string]$ExePath = ".\evrp",
    [string]$RootInstances = "instances",
    [string]$OutputRoot = "output\results_multigreedy"
)

function Invoke-Set-MultiGreedy {
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

    $csvPath = Join-Path $setOutputDir "${SetName}_multigreedy_results.csv"

    "set,instance,feasible_solutions_out_of_10,best_feasible_energy,best_overall_energy,best_feasible_routes,min_routes_overall,generation_ms" |
    Set-Content -Encoding UTF8 $csvPath

    Get-ChildItem -Path $instanceDir -Filter *.txt | Sort-Object Name | ForEach-Object {
        $instName = $_.Name
        $result = & $ExePath --multi-greedy $_.FullName

        $feasibleCount = 0
        $bestFeasibleEnergy = 999999.9
        $bestOverallEnergy = 999999.9
        $bestFeasibleRoutes = 0
        $minRoutesOverall = 99
        $generationMs = 0

        foreach ($line in $result) {
            if ($line -like "Solucion *") {
                # Example: Solucion 1 | factible: si | energia: 358.15 | emisiones: 308.904 | rutas: 2
                $parts = $line.Split("|") | ForEach-Object { $_.Trim() }
                
                $factiblePart = $parts[1] # factible: si
                $energiaPart = $parts[2] # energia: 358.15
                $rutasPart = $parts[4] # rutas: 2

                $factible = ($factiblePart -like "*si*")
                
                # Parse energy
                $energyVal = [double]($energiaPart.Replace("energia: ", "").Trim())
                
                # Parse routes
                $routesVal = [int]($rutasPart.Replace("rutas: ", "").Trim())

                if ($energyVal -lt $bestOverallEnergy) {
                    $bestOverallEnergy = $energyVal
                }
                if ($routesVal -lt $minRoutesOverall) {
                    $minRoutesOverall = $routesVal
                }

                if ($factible) {
                    $feasibleCount++
                    if ($energyVal -lt $bestFeasibleEnergy) {
                        $bestFeasibleEnergy = $energyVal
                        $bestFeasibleRoutes = $routesVal
                    }
                }
            }
            elseif ($line -like "*Tiempo total multi-greedy (ms):*") {
                $generationMs = [int]($line.Split(":")[-1].Trim())
            }
        }

        # Handle case where no solution is feasible
        $bestFeasibleEnergyStr = if ($bestFeasibleEnergy -eq 999999.9) { "N/A" } else { $bestFeasibleEnergy }
        $bestFeasibleRoutesStr = if ($bestFeasibleRoutes -eq 0) { "N/A" } else { $bestFeasibleRoutes }

        "$SetName,$instName,$feasibleCount,$bestFeasibleEnergyStr,$bestOverallEnergy,$bestFeasibleRoutesStr,$minRoutesOverall,$generationMs" |
        Add-Content -Encoding UTF8 $csvPath

        Write-Host "[$SetName] Procesada:" $instName " - Factibles: $feasibleCount/10"
    }

    $rows = Import-Csv $csvPath
    $feasibleInstancesCount = ($rows | Where-Object { [int]$_.feasible_solutions_out_of_10 -gt 0 }).Count
    $total = $rows.Count

    $summaryPath = Join-Path $setOutputDir "${SetName}_multigreedy_summary.txt"
    @(
        "set=$SetName"
        "instances=$total"
        "at_least_one_feasible_count=$feasibleInstancesCount"
        "at_least_one_feasible_pct=$([math]::Round((100.0 * $feasibleInstancesCount / $total), 2))"
        "csv=$csvPath"
    ) | Set-Content -Encoding UTF8 $summaryPath

    Write-Host ""
    Write-Host "CSV guardado en: $csvPath"
    Write-Host "Resumen guardado en: $summaryPath"
    Write-Host ""
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

Invoke-Set-MultiGreedy -SetName "small"
Invoke-Set-MultiGreedy -SetName "large"
