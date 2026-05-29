#include "unity.h"

static void (*g_setUp)(void)    = (void(*)(void))0;
static void (*g_tearDown)(void) = (void(*)(void))0;

void unity_suite_setup(void (*su)(void), void (*td)(void))
{
    g_setUp    = su;
    g_tearDown = td;
}

void setUp(void)    { if (g_setUp)    g_setUp(); }
void tearDown(void) { if (g_tearDown) g_tearDown(); }
