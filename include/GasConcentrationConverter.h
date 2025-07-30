#pragma once
#include <Arduino.h>
#include <math.h>

/**
 * @brief Helper class to convert gas concentrations from ppb to µg/m³
 */
class GasConcentrationConverter {
public:
    /**
     * @brief Convert O₃ (ozone) from ppb to µg/m³
     * @param o3_ppb Concentration in ppb
     * @param temperature_degc Temperature in °C
     * @param pressure_pa Pressure in Pascals
     * @return O₃ in µg/m³
     */
    static float convertO3PpbToUgPerM3(uint16_t o3_ppb, float temperature_degc, float pressure_pa);
    
    /**
     * @brief Convert NO₂ from ppb to µg/m³
     * @param no2_ppb Concentration in ppb
     * @param temperature_degc Temperature in °C
     * @param pressure_pa Pressure in Pascals
     * @return NO₂ in µg/m³
     */
    static float convertNO2PpbToUgPerM3(uint16_t no2_ppb, float temperature_degc, float pressure_pa);
    
    /**
     * @brief Calculate air density (kg/m³) using ideal gas law (optional utility)
     */
    static float calculateAtmosphericDensity(float temperature_degc, float pressure_pa);

private:
    static constexpr float R = 8.314459f;                 // J/(mol·K) - Universal gas constant
    static constexpr float O3_MOLECULAR_WEIGHT = 48.00f;  // g/mol
    static constexpr float NO2_MOLECULAR_WEIGHT = 46.01f; // g/mol
    static constexpr float AIR_MOLECULAR_WEIGHT = 28.97f; // g/mol - Average molecular weight of air
    static constexpr float KELVIN_OFFSET = 273.15f;       // Offset to convert °C to K
};