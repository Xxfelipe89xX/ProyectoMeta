import random
import math
import os
import json

GRID_SIZE = 200        
DEPOT_X = 100.0         
DEPOT_Y = 100.0         
DEMAND_MIN = 0.05       
DEMAND_MAX = 0.15       
BATTERY_CAPACITY = 110.0 
VEHICLE_CAPACITY = 3.0   
ALPHA = 0.0981           
BETA = 2.11              
EFF_M = 1.25            
EFF_D = 1.11            
EFF_P = 1.25            
EMISSION_RATE = 0.69     
SPEED_SET = [30, 40, 60, 80]  
CURB_WEIGHT = 2500      
MILES_TO_KM = 1.60934
MILES_TO_M = 1609.34

SEED = 2018 


def euclidean_distance_miles(p1, p2):
    """Euclidean distance between two points in miles."""
    return math.sqrt((p1[0] - p2[0])**2 + (p1[1] - p2[1])**2)


def generate_customers(n, rng):
    """Generate n customer locations uniformly in the grid."""
    coords = []
    for _ in range(n):
        x = round(rng.uniform(0, GRID_SIZE), 2)
        y = round(rng.uniform(0, GRID_SIZE), 2)
        coords.append((x, y))
    return coords


def generate_demands(n, rng):
    """Generate n customer demands uniformly between DEMAND_MIN and DEMAND_MAX tons."""
    return [round(rng.uniform(DEMAND_MIN, DEMAND_MAX), 4) for _ in range(n)]


def generate_stations(ns, rng):
    """Generate ns recharging station locations uniformly in the grid."""
    coords = []
    for _ in range(ns):
        x = round(rng.uniform(0, GRID_SIZE), 2)
        y = round(rng.uniform(0, GRID_SIZE), 2)
        coords.append((x, y))
    return coords


def generate_speed_matrix(n_vertices, rng):
    """Generate random speed assignments for each arc from SPEED_SET."""
    speeds = {}
    for i in range(n_vertices):
        for j in range(n_vertices):
            if i != j:
                speeds[(i, j)] = rng.choice(SPEED_SET)
    return speeds


def compute_energy_kwh(d_miles, speed_kmh, load_kg):
    """
    Compute energy consumption on an arc in kWh.
    E_ij = eff_d * eff_m * [alpha*(w+L)*d + beta*s^2*d]
    d in meters, s in m/s, load in kg
    """
    d_m = d_miles * MILES_TO_M
    s_ms = speed_kmh * 1000.0 / 3600.0 
    w_plus_L = CURB_WEIGHT + load_kg
    energy_joules = EFF_D * EFF_M * (ALPHA * w_plus_L * d_m + BETA * (s_ms ** 2) * d_m)
    energy_kwh = energy_joules / 3_600_000.0 
    return energy_kwh


def write_instance(instance, output_dir):
    name = instance['name']
    filepath = os.path.join(output_dir, f"{name}.txt")

    n_cust = instance['n_customers']
    n_stat = instance['n_stations']
    depot = instance['depot']
    customers = instance['customers']
    demands = instance['demands']
    stations = instance['stations']
    speeds = instance['speeds']
    max_vehicles = instance['max_vehicles']

    all_nodes = [depot] + customers + stations
    n_total = len(all_nodes)

    with open(filepath, 'w') as f:
        f.write(f"# EVRP Instance: {name}\n")
        f.write(f"# Generated following Zhang, Gajpal, Appadoo & Abdulkader (2018)\n")
        f.write(f"# Int. J. Production Economics, 203, 404-413\n")
        f.write(f"#\n")
        f.write(f"# Node indexing:\n")
        f.write(f"#   0          = Depot\n")
        f.write(f"#   1..{n_cust}       = Customers\n")
        f.write(f"#   {n_cust+1}..{n_cust+n_stat}     = Recharging Stations\n")
        f.write(f"#\n")
        f.write(f"# Coordinates in miles. Distances computed as Euclidean.\n")
        f.write(f"# Speeds in km/h. Demands in tons.\n")
        f.write(f"# Energy formula: E_ij = eff_d * eff_m * [alpha*(w+L)*d + beta*s^2*d]\n")
        f.write(f"#   with d in meters, s in m/s\n")
        f.write(f"\n")

        f.write(f"NAME: {name}\n")
        f.write(f"CUSTOMERS: {n_cust}\n")
        f.write(f"STATIONS: {n_stat}\n")
        f.write(f"TOTAL_NODES: {n_total}\n")
        f.write(f"VEHICLE_CAPACITY_TONS: {VEHICLE_CAPACITY}\n")
        f.write(f"BATTERY_CAPACITY_KWH: {BATTERY_CAPACITY}\n")
        f.write(f"CURB_WEIGHT_KG: {CURB_WEIGHT}\n")
        f.write(f"ALPHA: {ALPHA}\n")
        f.write(f"BETA: {BETA}\n")
        f.write(f"EFF_MOTOR: {EFF_M}\n")
        f.write(f"EFF_BATTERY_DISCHARGE: {EFF_D}\n")
        f.write(f"EFF_RECHARGE: {EFF_P}\n")
        f.write(f"EMISSION_RATE_KG_PER_KWH: {EMISSION_RATE}\n")
        f.write(f"MAX_VEHICLES: {max_vehicles}\n")
        f.write(f"GRID_SIZE_MILES: {GRID_SIZE}\n")
        f.write(f"MILES_TO_METERS: {MILES_TO_M}\n")
        f.write(f"\n")

        # Node coordinates
        f.write(f"NODE_COORD_SECTION\n")
        f.write(f"# NodeID  Type       X_miles   Y_miles   Demand_tons\n")
        # Depot
        f.write(f"0\tD\t{depot[0]:.2f}\t{depot[1]:.2f}\t0.0000\n")
        # Customers
        for i in range(n_cust):
            f.write(f"{i+1}\tC\t{customers[i][0]:.2f}\t{customers[i][1]:.2f}\t{demands[i]:.4f}\n")
        # Stations
        for i in range(n_stat):
            f.write(f"{n_cust+1+i}\tS\t{stations[i][0]:.2f}\t{stations[i][1]:.2f}\t0.0000\n")
        f.write(f"\n")

        f.write(f"DISTANCE_MATRIX_MILES\n")
        for i in range(n_total):
            row_vals = []
            for j in range(n_total):
                if i == j:
                    row_vals.append("0.0000")
                else:
                    d = euclidean_distance_miles(all_nodes[i], all_nodes[j])
                    row_vals.append(f"{d:.4f}")
            f.write("\t".join(row_vals) + "\n")
        f.write(f"\n")

        f.write(f"SPEED_MATRIX_KMH\n")
        for i in range(n_total):
            row_vals = []
            for j in range(n_total):
                if i == j:
                    row_vals.append("0")
                else:
                    row_vals.append(str(speeds[(i, j)]))
            f.write("\t".join(row_vals) + "\n")
        f.write(f"\n")

        f.write(f"EOF\n")

    return filepath


def write_instance_json(instance, output_dir):
    """Write instance to a JSON file for easier programmatic use."""
    name = instance['name']
    filepath = os.path.join(output_dir, f"{name}.json")

    n_cust = instance['n_customers']
    n_stat = instance['n_stations']
    all_nodes = [instance['depot']] + instance['customers'] + instance['stations']
    n_total = len(all_nodes)

    dist_matrix = []
    for i in range(n_total):
        row = []
        for j in range(n_total):
            if i == j:
                row.append(0.0)
            else:
                row.append(round(euclidean_distance_miles(all_nodes[i], all_nodes[j]), 4))
        dist_matrix.append(row)

    speed_matrix = []
    for i in range(n_total):
        row = []
        for j in range(n_total):
            if i == j:
                row.append(0)
            else:
                row.append(instance['speeds'][(i, j)])
        speed_matrix.append(row)

    data = {
        "name": name,
        "n_customers": n_cust,
        "n_stations": n_stat,
        "total_nodes": n_total,
        "parameters": {
            "vehicle_capacity_tons": VEHICLE_CAPACITY,
            "battery_capacity_kwh": BATTERY_CAPACITY,
            "curb_weight_kg": CURB_WEIGHT,
            "alpha": ALPHA,
            "beta": BETA,
            "eff_motor": EFF_M,
            "eff_battery_discharge": EFF_D,
            "eff_recharge": EFF_P,
            "emission_rate_kg_per_kwh": EMISSION_RATE,
            "max_vehicles": instance['max_vehicles'],
            "grid_size_miles": GRID_SIZE,
            "miles_to_meters": MILES_TO_M,
        },
        "depot": {"id": 0, "x": instance['depot'][0], "y": instance['depot'][1]},
        "customers": [
            {"id": i + 1, "x": instance['customers'][i][0], "y": instance['customers'][i][1],
             "demand_tons": instance['demands'][i]}
            for i in range(n_cust)
        ],
        "stations": [
            {"id": n_cust + 1 + i, "x": instance['stations'][i][0], "y": instance['stations'][i][1]}
            for i in range(n_stat)
        ],
        "distance_matrix_miles": dist_matrix,
        "speed_matrix_kmh": speed_matrix,
    }

    with open(filepath, 'w') as f:
        json.dump(data, f, indent=2)

    return filepath


def main():
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    small_dir = os.path.join(base_dir, "instances", "small")
    large_dir = os.path.join(base_dir, "instances", "large")
    json_dir = os.path.join(base_dir, "instances", "json")
    os.makedirs(small_dir, exist_ok=True)
    os.makedirs(large_dir, exist_ok=True)
    os.makedirs(json_dir, exist_ok=True)

    master_rng = random.Random(SEED)


    print("=" * 60)
    print("Generating SMALL SIZE instances (C10R2 - C24R2)")
    print("=" * 60)

    small_instances = []
    for n_cust in range(10, 25): 
        name = f"C{n_cust}R2"
        n_stat = 2

        rng_seed = master_rng.randint(0, 2**31)
        rng = random.Random(rng_seed)

        customers = generate_customers(n_cust, rng)
        demands = generate_demands(n_cust, rng)
        stations = generate_stations(n_stat, rng)
        n_total = 1 + n_cust + n_stat  
        speeds = generate_speed_matrix(n_total, rng)

        total_demand = sum(demands)
        max_vehicles = math.ceil(total_demand / VEHICLE_CAPACITY) + 2

        instance = {
            'name': name,
            'n_customers': n_cust,
            'n_stations': n_stat,
            'depot': (DEPOT_X, DEPOT_Y),
            'customers': customers,
            'demands': demands,
            'stations': stations,
            'speeds': speeds,
            'max_vehicles': max_vehicles,
        }

        txt_path = write_instance(instance, small_dir)
        json_path = write_instance_json(instance, json_dir)
        small_instances.append(instance)
        print(f"  {name}: {n_cust} customers, {n_stat} stations, "
              f"total demand = {total_demand:.3f} tons, max vehicles = {max_vehicles}")


    print("\n" + "=" * 60)
    print("Generating LARGE SIZE instances")
    print("=" * 60)

    customer_counts = [25, 50, 75, 100, 150]
    station_counts = [2, 4, 6, 8]

    large_instances = []

    for n_cust in customer_counts:
      
        cust_seed = master_rng.randint(0, 2**31)
        cust_rng = random.Random(cust_seed)
        base_customers = generate_customers(n_cust, cust_rng)
        base_demands = generate_demands(n_cust, cust_rng)

        for n_stat in station_counts:
            for variant in [1, 2]:
                name = f"C{n_cust}R{n_stat}-{variant}"

                var_seed = master_rng.randint(0, 2**31)
                var_rng = random.Random(var_seed)

                stations = generate_stations(n_stat, var_rng)
                n_total = 1 + n_cust + n_stat
                speeds = generate_speed_matrix(n_total, var_rng)

                total_demand = sum(base_demands)
                max_vehicles = math.ceil(total_demand / VEHICLE_CAPACITY) + 2

                instance = {
                    'name': name,
                    'n_customers': n_cust,
                    'n_stations': n_stat,
                    'depot': (DEPOT_X, DEPOT_Y),
                    'customers': list(base_customers),  
                    'demands': list(base_demands),      
                    'stations': stations,
                    'speeds': speeds,
                    'max_vehicles': max_vehicles,
                }

                txt_path = write_instance(instance, large_dir)
                json_path = write_instance_json(instance, json_dir)
                large_instances.append(instance)
                print(f"  {name}: {n_cust} customers, {n_stat} stations, "
                      f"total demand = {total_demand:.3f} tons, max vehicles = {max_vehicles}")


    print("\n" + "=" * 60)
    print("GENERATION SUMMARY")
    print("=" * 60)
    print(f"Small instances: {len(small_instances)} (saved to {small_dir})")
    print(f"Large instances: {len(large_instances)} (saved to {large_dir})")
    print(f"JSON versions:   saved to {json_dir}")
    print(f"\nTotal instances: {len(small_instances) + len(large_instances)}")
    print(f"\nParameters used:")
    print(f"  Grid:               {GRID_SIZE} x {GRID_SIZE} miles")
    print(f"  Depot:              ({DEPOT_X}, {DEPOT_Y}) miles")
    print(f"  Demand range:       [{DEMAND_MIN}, {DEMAND_MAX}] tons")
    print(f"  Battery capacity:   {BATTERY_CAPACITY} kWh")
    print(f"  Vehicle capacity:   {VEHICLE_CAPACITY} tons")
    print(f"  Curb weight:        {CURB_WEIGHT} kg")
    print(f"  Alpha (arc const):  {ALPHA}")
    print(f"  Beta (veh const):   {BETA}")
    print(f"  eff_m (motor):      {EFF_M}")
    print(f"  eff_d (discharge):  {EFF_D}")
    print(f"  eff_p (recharge):   {EFF_P}")
    print(f"  Speed set:          {SPEED_SET} km/h")
    print(f"  Random seed:        {SEED}")


if __name__ == "__main__":
    main()
