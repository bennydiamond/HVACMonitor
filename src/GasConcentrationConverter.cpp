#include "GasConcentrationConverter.h"

float GasConcentrationConverter::convertO3PpbToUgPerM3(uint16_t o3_ppb, float temperature_degc, float pressure_pa) {
    if (o3_ppb == 0) return 0.0f;
    
    float temperature_k = temperature_degc + KELVIN_OFFSET;
    
    // Formula: µg/m³ = (ppb × MW × P) / (R × T) × 10^-3
    // MW is in g/mol, so we multiply by 10^-3 to convert g to µg
    // This gives us the correct µg/m³ units
    return (o3_ppb * O3_MOLECULAR_WEIGHT * pressure_pa * 1e-3f) / (R * temperature_k);
}

float GasConcentrationConverter::convertNO2PpbToUgPerM3(uint16_t no2_ppb, float temperature_degc, float pressure_pa) {
    if (no2_ppb == 0) return 0.0f;
    
    float temperature_k = temperature_degc + KELVIN_OFFSET;
    
    // Formula: µg/m³ = (ppb × MW × P) / (R × T) × 10^-3
    // MW is in g/mol, so we multiply by 10^-3 to convert g to µg
    // This gives us the correct µg/m³ units
    return (no2_ppb * NO2_MOLECULAR_WEIGHT * pressure_pa * 1e-3f) / (R * temperature_k);
}

float GasConcentrationConverter::calculateAtmosphericDensity(float temperature_degc, float pressure_pa) {
    float temperature_k = temperature_degc + KELVIN_OFFSET;
    float air_molecular_weight = AIR_MOLECULAR_WEIGHT; // g/mol
    
    // Density = (P × MW) / (R × T)
    // MW in g/mol, divide by 1000 to convert to kg/mol for kg/m³ result
    float density = (pressure_pa * air_molecular_weight) / (R * temperature_k * 1000.0f);
    return density;
}

// Alternative implementation with clearer unit conversion:
/*
float GasConcentrationConverter::convertO3PpbToUgPerM3(uint16_t o3_ppb, float temperature_degc, float pressure_pa) {
    if (o3_ppb == 0) return 0.0f;
    
    float temperature_k = temperature_degc + KELVIN_OFFSET;
    
    // Step by step for clarity:
    // 1. Convert ppb to molar concentration: mol/m³ = (ppb × P) / (R × T × 10^9)
    // 2. Convert to mass concentration: g/m³ = mol/m³ × MW
    // 3. Convert to µg/m³: µg/m³ = g/m³ × 10^6
    // Combined: µg/m³ = (ppb × P × MW × 10^6) / (R × T × 10^9) = (ppb × P × MW) / (R × T × 1000)
    
    return (o3_ppb * O3_MOLECULAR_WEIGHT * pressure_pa) / (R * temperature_k * 1000.0f);
}
*/