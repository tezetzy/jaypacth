#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>
#include <dlfcn.h>
#include <vector>
#include <cctype>
#include <array>
#include <utility>

#include "AArch64_ModHelper/ARMv8_ASMHelper.h"
    #include "GTASA_STRUCTS_210.h"

MYMODCFG(net.jaysiii.aml, Logika, 1.0, jaysiii)

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Saves     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uintptr_t pGTASA;
void* hGTASA;
static constexpr float fMagic = 50.0f / 30.0f;

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Vars      ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
float *ms_fTimeStep;
CPlayerInfo* WorldPlayers;

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Hooks     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
extern "C" void addad(void)
{
    asm("VMOV.F32 S0, #0.5");
}
DECL_HOOKv(CameraProcess_HighFPS, void* self)
{
    float DrunkRotationBak = *DrunkRotation;
    CameraProcess_HighFPS(self);
    if(DrunkRotationBak != *DrunkRotation)
    {
        *DrunkRotation = DrunkRotationBak + 5.0f * GetTimeStepMagic();
    }
}
DECL_HOOKv(ControlGunMove, void* self, CVector2D* vec2D) // AimingRifleWalkFix
{
    float save = *ms_fTimeStep; *ms_fTimeStep = fMagic;
    ControlGunMove(self, vec2D);
    *ms_fTimeStep = save;
}
float fWideScreenWidthScale, fWideScreenHeightScale;
DECL_HOOKv(DrawCrosshair)
{
    static constexpr float XSVal = 1024.0f / 1920.0f; // prev. 0.530, now it's 0.533333..3
    static constexpr float YSVal = 768.0f / 1920.0f; // unchanged :p
}
DECL_HOOKv(CalculateAspectRatio_CrosshairFix)
{
    CalculateAspectRatio_CrosshairFix();

    fWideScreenWidthScale = 640.0f / (*ms_fAspectRatio * 448.0f);
    fWideScreenHeightScale = 448.0 / 448.0f;
}
uintptr_t DrawMoney_BackTo;
extern "C" const char* DrawMoney_Patch(int isPositive)
{
    static const char* positiveT = "$%08d";
    static const char* negativeT = "-$%07d";
    
    return isPositive ? positiveT : negativeT;
}
__attribute__((optnone)) __attribute__((naked)) void DrawMoney_Inject(void)
{
    asm volatile(
        "LDR R5, [R0]\n"
        "SMLABB R0, R1, R11, R5\n"
        "PUSH {R0-R11}\n"
        "LDR R0, [SP, #8]\n"
        "BL DrawMoney_Patch\n"
        "STR R0, [SP, #4]\n");
    asm volatile(
        "MOV R12, %0\n"
        "POP {R0-R11}\n"
        "BX R12\n"
    :: "r" (DrawMoney_BackTo));
}
DECL_HOOK(ScriptHandle, GenerateNewPickup, float x, float y, float z, int16_t modelId, ePickupType pickupType, int ammo, int16_t moneyPerDay, bool isEmpty, const char* msg)
{
    if(modelId == 1277 && x == 1263.05f && y == -773.67f && z == 1091.39f)
    {
        return GenerateNewPickup(1291.2f, -798.0f, 1089.39f, modelId, pickupType, ammo, moneyPerDay, isEmpty, msg);
    }
    return GenerateNewPickup(x, y, z, modelId, pickupType, ammo, moneyPerDay, isEmpty, msg);
}

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
extern "C" void OnModLoad()
{
    pGTASA = aml->GetLib("libGTASA.so");
    hGTASA = dlopen("libGTASA.so", RTLD_LAZY);
    
    // Variables Start //
    SET_TO(ms_fTimeStep, aml->GetSym(hGTASA, "_ZN6CTimer12ms_fTimeStepE"));
    SET_TO(WorldPlayers, aml->GetSym(hGTASA, "_ZN6CWorld7PlayersE"));
    // Variables End   //

    // Fix walking while rifle-aiming
    if(cfg->Bind("FixAimingWalkRifle", true, "Gameplay")->GetBool())
    {
        HOOKPLT(ControlGunMove, pGTASA + 0x66F9D0);
    }
    if(cfg->GetBool("HighFPSAimingWalkingFix", true, "Gameplay"))
    {
        aml->Unprot(pGTASA + 0x4DD9E8, sizeof(float));
        *(float*)(pGTASA + 0x4DD9E8) = 0.015f;
        //SET_TO(float_4DD9E8, pGTASA + 0x4DD9E8);
        //HOOK(TaskSimpleUseGunSetMoveAnim, aml->GetSym(hGTASA, "_ZN17CTaskSimpleUseGun11SetMoveAnimEP4CPed"));
    }
    if(cfg->GetBool("FixCrosshair", true, "Visual"))
    {
        HOOKPLT(DrawCrosshair, pGTASA + 0x672880);
        //HOOK(CalculateAspectRatio_CrosshairFix, aml->GetSym(hGTASA, "_ZN5CDraw20CalculateAspectRatioEv"));
    }
    if(cfg->GetBool("PCStyledMoney", false, "Visual"))
    {
        DrawMoney_BackTo = pGTASA + 0x2BD260 + 0x1;
        aml->Redirect(pGTASA + 0x2BD258 + 0x1, (uintptr_t)DrawMoney_Inject);
    }
    if(cfg->GetBool("AllowCrouchWith1HP", true, "Gameplay"))
    {
        aml->Write(pGTASA + 0x54316C, "\xB4", 1);
    }
    if(cfg->GetBool("FixDrunkCameraHighFPS", true, "Visual"))
    {
        HOOKPLT(CameraProcess_HighFPS, pGTASA + 0x6717BC);
    }

}
