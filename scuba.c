/** \file scuba.c

 $COPYRIGHT: $
 -    Copyright 2017 Barr Group, LLC. All rights reserved.
**/

#include <stdint.h>
#include "os.h"
#include "scuba.h"


#define RMV   (1200UL)        /**< Respiratory minute volume = 1200 centiLitres / minute */
#define RHSV (RMV / 120UL)    /**< Respiratory half second volume = 10 centiLitres / half_second */


/**
 FUNCTION: gas_rate_in_cl

 DESCRIPTION:
 This computes how much gas is consumed in a half second at a certain depth.

 PARAMETERS:
 -    The current depth in meters

 RETURNS:
 -    The number of centilitres in gas.

 NOTES:


**/
uint32_t gas_rate_in_cl(uint32_t depth_in_mm)
{
  uint32_t  depth_in_m = (depth_in_mm / 1000UL);    

   // 10m of water = 1 bar = 100 centibar
   uint16_t ambient_pressure_in_cb = 100UL + (10UL * depth_in_m);    
            
   // Gas consumed at STP = RHSV * ambient pressure / standard pressure
   return ((RHSV * ambient_pressure_in_cb) / 100UL);
}

/**
 FUNCTION: gas_to_surface_in_cl

 DESCRIPTION:
 This computes how much gas at STP it would take to surface from the current
 depth, assuming no decompression stops and an ascent rate of ASCENT_RATE_LIMIT.

 It does this via numerical integration. The step size is 1 m.

 PARAMETERS:
 -    The current depth in meters

 RETURNS:
 -    The number of centilitres of gas at STP required to make it to the surface.

 NOTES:


**/
uint32_t
gas_to_surface_in_cl(uint32_t depth_in_mm)
{
  uint32_t    gas = 0UL;
  uint16_t    halfsecs_to_ascend_1m = (2UL * 60UL) / ASCENT_RATE_LIMIT;;
  uint16_t    ambient_pressure_in_cb;        /* Ambient pressure in centiBar */

  for (uint32_t depth_in_m = depth_in_mm / 1000UL; depth_in_m > 0UL; depth_in_m--)
  {
    ambient_pressure_in_cb = 100UL + ((depth_in_m * 100UL) / 10UL);
    gas += ((RHSV * halfsecs_to_ascend_1m * ambient_pressure_in_cb) / 100UL);
  }

  return (gas);
}
