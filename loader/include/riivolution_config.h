/*
 * Riivolution XML Configuration Parser
 * Nintendont integration
 */

#ifndef _RIIVOLUTION_CONFIG_H_
#define _RIIVOLUTION_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// Initialize Riivolution XML parser
int RiiConfig_Init(void);

// Load and parse Riivolution XML from SD/USB
int RiiConfig_LoadXML(const char* filepath);

// Clean up parser resources
void RiiConfig_Cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // _RIIVOLUTION_CONFIG_H_
