#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>
#include <dlfcn.h>

#include <sys/system_properties.h>

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
void Redirect(uintptr_t addr, uintptr_t to)
{
    if(!addr) return;
    if(addr & 1)
    {
        addr &= ~1;
        if (addr & 2)
        {
            aml->PlaceNOP(addr, 1);
            addr += 2;
        }
        uint32_t hook[2];
        hook[0] = 0xF000F8DF;
        hook[1] = to;
        aml->Write(addr, (uintptr_t)hook, sizeof(hook));
    }
    else
    {
        uint32_t hook[2];
        hook[0] = 0xE51FF004;
        hook[1] = to;
        aml->Write(addr, (uintptr_t)hook, sizeof(hook));
    }
}
void (*_rwOpenGLSetRenderState)(RwRenderState, int);
void (*_rwOpenGLGetRenderState)(RwRenderState, void*);
void (*ClearPedWeapons)(CPed*);

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Hooks     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
extern "C" void adadad(void)
{
    asm("VMOV.F32 S0, #0.5");
}
DECL_HOOKv(EntityRender, CEntity* ent)
{
    if(ent->m_nType == ENTITY_TYPE_VEHICLE)
    {
        EntityRender(ent);
        return;
    }

    //RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLNONE);
    EntityRender(ent);
    //RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLBACK);
}
int moon_alphafunc, moon_vertexblend;
uintptr_t MoonVisual_1_BackTo;
extern "C" void MoonVisual_1(void)
{
    _rwOpenGLGetRenderState(rwRENDERSTATEALPHATESTFUNCTION, &moon_alphafunc);
    _rwOpenGLGetRenderState(rwRENDERSTATEVERTEXALPHAENABLE, &moon_vertexblend);
    _rwOpenGLSetRenderState(rwRENDERSTATEALPHATESTFUNCTION, rwALPHATESTFUNCTIONALWAYS);
    _rwOpenGLSetRenderState(rwRENDERSTATEVERTEXALPHAENABLE, true);

    _rwOpenGLSetRenderState(rwRENDERSTATESRCBLEND, rwBLENDSRCALPHA);
    _rwOpenGLSetRenderState(rwRENDERSTATEDESTBLEND, rwBLENDZERO);
}
__attribute__((optnone)) __attribute__((naked)) void MoonVisual_1_inject(void)
{
    asm volatile(
        "push {r0-r11}\n"
        "bl MoonVisual_1\n");
    asm volatile(
        "mov r12, %0\n"
        "pop {r0-r11}\n"
        "bx r12\n"
    :: "r" (MoonVisual_1_BackTo));
}
uintptr_t MoonVisual_2_BackTo;
extern "C" void MoonVisual_2(void)
{
    _rwOpenGLSetRenderState(rwRENDERSTATEALPHATESTFUNCTION, moon_alphafunc);
    _rwOpenGLSetRenderState(rwRENDERSTATEVERTEXALPHAENABLE, moon_vertexblend);
    _rwOpenGLSetRenderState(rwRENDERSTATESRCBLEND, rwBLENDONE);
    _rwOpenGLSetRenderState(rwRENDERSTATESRCBLEND, rwBLENDDESTALPHA);
    _rwOpenGLSetRenderState(rwRENDERSTATEZWRITEENABLE, false);
}
__attribute__((optnone)) __attribute__((naked)) void MoonVisual_2_inject(void)
{
    asm volatile(
        "push {r0-r11}\n"
        "bl MoonVisual_2\n");
    asm volatile(
        "mov r12, %0\n"
        "pop {r0-r11}\n"
        "bx r12\n"
    :: "r" (MoonVisual_2_BackTo));
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
DECL_HOOKv(ProcessBuoyancy, void* self, CPhysical* phy, float unk, CVector* vec1, CVector* vec2) // BuoyancySpeedFix
{
    if(phy->m_nType == ENTITY_TYPE_PED && ((CPed*)phy)->m_nPedType == PED_TYPE_PLAYER1)
    {
        float save = *ms_fTimeStep; *ms_fTimeStep = (1.0f + ((save / fMagic) / 1.5f)) * (save / fMagic);
        ProcessBuoyancy(self, phy, unk, vec1, vec2);
        *ms_fTimeStep = save;
        return;
    }
    ProcessBuoyancy(self, phy, unk, vec1, vec2);
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

    // Functions Start //
    SET_TO(_rwOpenGLSetRenderState, aml->GetSym(hGTASA, "_Z23_rwOpenGLSetRenderState13RwRenderStatePv"));
    SET_TO(_rwOpenGLGetRenderState, aml->GetSym(hGTASA, "_Z23_rwOpenGLGetRenderState13RwRenderStatePv"));
    SET_TO(ClearPedWeapons, aml->GetSym(hGTASA, "_ZN4CPed12ClearWeaponsEv"));
    // Functions End   //
    
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
