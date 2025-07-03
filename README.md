# DamFlow
 2024/2025 Mobile and Pervasive Computing Group Project  


Glossary of Variables:  

**NOTE:** Every distance measure in the project is applied to the sensors in 'cm' but should be simulated into 'm' in any user interface.  
  
*Higher Water Threshold* - Float representing the upper limit for the water level in the dam. If the water reaches this level the hatch will automatically open if closed.  
*Lower Water Threshold* - Float representing the lower limit for the water level in the dam. If the water reaches this level the hatch will automatically close if open.  
*Request to open hatch* - Boolean representing the intent to open the dam hatch. If true, and the water level is above the lower limit, then the hatch will open. Otherwise the hatch will stay closed.  
*Hatch is Open* - Boolean representing the current state of the dam hatch. If true, then the hatch is open. Otherwise the hatch is closed.  
*Request to sound buzzer* - Boolean representing the intent to sound the buzzer. If true, then the buzzer will sound. Otherwise the buzzer will be shut.  
*Buzzer is On* - Boolean representing the current state of the buzzer. If true, then the buzzer is on. Otherwise the buzzer is off.  
*Infrared is Triggered*  - Boolean representing the current state of infrared sensor. If true, then the sensor is triggered. Otherwise the buzzer is not triggered.  
  
*Water level history* - Directory for the history of observed water levels. The entries are organized by their UNIX timestamp, and carry two extra fields:  
  *level* - Float representing the water level measurement at the time of the current entry.  
		*hatch status* - String representing the status of the hatch at the time of the current entry. "Open" if the hatch was open at the time, "Closed" otherwise.  
  
*Current Water Level* - Directory for the current observed water level. The entry has 4 extra fields:  
  *level* - same as above.  
		*hatch status* - same as above.  
		*timestamp* - UNIX timestamp representing the time of the current entry.  
		*datetime* - String representing the date and time of the current entry for better representation in user interface. It is in the format: "yyyy-mm-dd hh:mm:ss".  
		
		
**Realtime Database Structure:**  
  
Higher Water Threshold: "/esp/higher_water_threshold"  <- (Float)  
Lower Water Threshold: "/esp/lower_water_threshold"  <- (Float)  
Request to open hatch: "/esp/open_hatch_request"  <- (Bool)  
Hatch is Open: "/esp/hatch_is_open"  <- (Bool)  
Request to sound buzzer: "/esp/sound_buzzer_request"  <- (Bool)  
Buzzer is On: "/esp/buzzer_is_on"  <- (Bool)  
Infrared is Triggered: "esp/ir_is_triggered"  <- (Bool)  
  
Water level history: "esp/water_levels/" + (UNIX timestamp) <- Json  
 Json: {  
   level: (Float),  
   hatch_status: (String)  
   }  
  
Current water level: "esp/current_water_level"  
 Json: {  
  level: (Float),  
  hatch_status: (String),  
  timestamp (Unsigned Long),  
  datetime (String)  
  }  
