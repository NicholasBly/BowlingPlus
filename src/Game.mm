#import <UIKit/UIKit.h>
#import "BFShared.h"
#import "Il2Cpp.h"
#include <math.h>
#include <string>
#include <vector>
#include "KegelParse.h"
#include <ctype.h>
#include <string.h>
#include <sys/utsname.h>
#include <vector>

using namespace IL;

// GameParams._playerLocation (enum PLAYER_STATE in the game)
enum { LOC_UPPER_SCREEN = 0, LOC_BALL_RETURNER = 1, LOC_BOTTOM_MONITOR = 2, LOC_START_POS = 3,
       LOC_THROWING = 4, LOC_ON_PINDECK = 5, LOC_REPLAYER = 6 };
// GameParams.gameMode (enum GameModes). FUN = Practice ("Play Now" in the code, local session).
// COMPETE = online head-to-head, NONE = tournaments/menus, TUTORIAL = tutorial.
enum { MODE_FUN = 0, MODE_COMPETE = 2, MODE_NONE = 4, MODE_TUTORIAL = 5 };
// UnityEngine.CollisionDetectionMode
enum { CDM_DISCRETE = 0, CDM_CONTINUOUS_DYNAMIC = 2, CDM_SPECULATIVE = 3 };
// Sweep-based CCD (ContinuousDynamic) only follows straight-line motion. Speculative CCD also
// accounts for spin, which can help when a tumbling pin's top swings into another pin, so it's
// an experimental option for PINS ONLY. Never the ball: the ball spins at ~600 rpm, so speculative
// contacts reach far ahead of it and "ghost" contacts with the pin deck and pins launched it into
// the air (1.2.0 bug).
static int PinCDM()  { return gBF.pinSpec ? CDM_SPECULATIVE : CDM_CONTINUOUS_DYNAMIC; }
static int BallCDM() { return CDM_CONTINUOUS_DYNAMIC; }
// DLC_TYPE.DLC_TEXTURE: the game's id for "3D ball skin file"
enum { DLC_TEXTURE = 3 };

// ---------------------------------------------------------------------------
// Names we look up once the game has loaded
// ---------------------------------------------------------------------------
static struct {
    bool ok;
    Il2CppClass *RunPsycsTest, *GameParams, *PinHolder, *Pin, *LoadDLCTex, *InvHelper, *ItemHolder, *ShopItem, *Item, *Arsenal, *InventaryData, *Tutorial;
    Il2CppClass *UObject, *GameObject, *Component, *Rigidbody, *Renderer, *Material, *Texture2D, *AssetBundle, *ImageConversion, *Byte;
    Il2CppObject *tRunPsycsTest, *tPinHolder, *tRigidbody, *tRenderer, *tLoadDLCTex, *tArsenal, *tTexture2D, *tAssetBundle, *tTutorial;
    const MethodInfo *FindObjectsOfType, *FindObjectsOfTypeAll, *GetName, *SetHideFlags, *GetGameObject, *GetComponent, *GetComponentInChildren;
    const MethodInfo *RB_isKinematic, *RB_getCDM, *RB_setCDM, *RB_getVel, *RB_setVel, *RB_setMaxAngVel, *RB_getLinDamp, *RB_getAngDamp;
    const MethodInfo *R_getMaterial, *M_getMainTex;
    const MethodInfo *T2D_ctor, *LoadImage, *AB_LoadFromFile, *AB_LoadAsset, *AB_Unload;
    const MethodInfo *RPT_UpdatePinPositions, *LDT_textureLoaded, *LDT_loadForItem, *IH_GetShopItemById, *SI_getDLC, *Ars_InitScrollData;
    const MethodInfo *InvD_InitBallsOnReturner, *TM_IsInTutorial, *TM_SkipTutorial, *CSM_TutorialDone;
    Il2CppClass *CSM, *IVD, *Constants, *Application, *MonoCoreLoop, *InitState, *LoadingWindow;
    Il2CppObject *tMonoCoreLoop, *tLoadingWindow;
    const MethodInfo *GO_activeInHierarchy, *LW_Switch;
    int mcl_sm, sm_state, is_gdpr, lw_conn, lw_noConn, lw_login, lw_online, lw_offline;
    Il2CppClass *Connector, *Reconnect, *FloatWnd, *GameLogger, *Processing;
    Il2CppClass *OilGen, *OilDescData, *Drawer, *PracticeMgr, *Replayer;
    Il2CppObject *tOilGen, *tOilDescData, *tReplayer;
    const MethodInfo *OG_reloadCurrent, *OG_reloadRedraw, *OG_showOnLane, *OG_isActive, *D_drawFromFile, *D_graphF, *D_graphR, *OG_texForId, *OG_reloadById, *OG_colorData, *OG_genRG16, *G_getKeys, *G_setKeys, *OG_updateColor;
    const MethodInfo *R_getSharedMat, *M_hasProp, *M_getFloatI, *M_setFloatS, *M_getTexI, *Sh_propToId, *T_getWrap, *T_setWrap;
    int og_lines, ri_oilTex;
    const MethodInfo *Proc_Hide;
    int proc_count;
    Il2CppObject *tFloatWnd;
    const MethodInfo *App_reach;
    const MethodInfo *App_getFps, *App_setFps;
    int tm_activeStage;
    Il2CppObject *tIVD;
    const MethodInfo *IVD_GetItemDataByID;
    int ivd_items;
    int rpt_sphere, rpt_sphereRender, rpt_kegsUp, rpt_kegsUpdate;
    int pin_visual, pin_hq, pin_lq, pin_mirror;
    const MethodInfo *M_setMainTex;
    const MethodInfo *TA_getText;
    // tapping the game's pin layouts
    const MethodInfo *Scr_w, *Scr_h, *Cv_mode, *Cv_cam, *Cv_root, *RT_corners, *Cam_w2s, *Cam_all, *Cam_depth, *Cam_mask,
                     *Cam_target, *Cam_main, *Beh_enabled, *GO_activeH, *GO_layer, *GO_inParent, *Col_bounds, *RTO_render;
    Il2CppClass *Vec3Cls;
    Il2CppObject *tBoxCollider, *tCanvas, *tMMBM;
    int mmbm_pinObj, mmbm_pinBack, rpt_renderMon;
    const MethodInfo *RB_getAngVel, *RB_setAngVel, *RB_getMaxAng, *Txt_get, *Txt_set;
    int rpt_rpmText;
    Il2CppClass *InvData;
    Il2CppObject *tInvData;
    int inv_pinMat, inv_mirrorMat, inv_rpmFactor, inv_maxOmega, inv_maxRpm;
    const MethodInfo *Comp_getTransform, *GO_getTransform, *Tr_getRot, *Tr_setRot, *Tr_getLocalRot, *Tr_setLocalRot;
    int ph_pins, pin_physic, ldt_dlcID, ldt_toChange, ldt_objectID, item_itemId, item_baseId, shop_name, si_dlcLink;
    int ih_shop, ars_ballsData, ars_scroll;
    FieldInfo *gp_gameMode, *gp_location, *inv_currentBall, *ih_instance, *invd_instance;
} N;

static int Off(Il2CppClass *k, const char *name) {
    int o = k ? FieldOffset(k, name) : -1;
    if (k && o < 0) BFLog(@"field not found: %s", name);
    return o;
}

static const MethodInfo *Meth(Il2CppClass *k, const char *name, int argc, const char *p0 = nullptr, const char *p1 = nullptr) {
    const MethodInfo *m = k ? FindMethod(k, name, argc, p0, p1) : nullptr;
    if (k && !m) BFLog(@"method not found: %s", name);
    return m;
}

static bool Resolve() {
    N.RunPsycsTest = FindClass("", "RunPsycsTest");
    N.GameParams   = FindClass("", "GameParams");
    N.UObject      = FindClass("UnityEngine", "Object");
    N.GameObject   = FindClass("UnityEngine", "GameObject");
    N.Component    = FindClass("UnityEngine", "Component");
    N.Rigidbody    = FindClass("UnityEngine", "Rigidbody");
    if (!N.RunPsycsTest || !N.GameParams || !N.UObject || !N.GameObject || !N.Component || !N.Rigidbody) return false;

    N.PinHolder   = FindClass("", "PinHolder");
    N.Pin         = FindClass("", "Pin");
    N.LoadDLCTex  = FindClass("", "LoadDLCContentTexture");
    N.InvHelper   = FindClass("Managers", "InventoryHelper");
    N.ItemHolder  = FindClass("Managers", "ItemHolder");
    N.ShopItem    = FindClass("Models", "mdl_Shop_Item");
    N.Item        = FindClass("Models", "mdl_Item");
    N.Arsenal     = FindClass("Bowling.GUI", "ArsenalBallManager");
    N.Renderer    = FindClass("UnityEngine", "Renderer");
    N.Material    = FindClass("UnityEngine", "Material");
    N.Texture2D   = FindClass("UnityEngine", "Texture2D");
    N.AssetBundle = FindClass("UnityEngine", "AssetBundle");
    N.ImageConversion = FindClass("UnityEngine", "ImageConversion");
    N.Byte        = FindClass("System", "Byte");
    N.InventaryData = FindClass("", "InventaryData");
    N.Tutorial    = FindClass("Bowling.GUI", "TutorialManager");
    N.CSM         = FindClass("", "ClientSideManager");
    N.Constants   = FindClass("Client.Core", "Constants");
    N.Application = FindClass("UnityEngine", "Application");
    N.App_getFps  = Meth(N.Application, "get_targetFrameRate", 0);
    N.App_setFps  = Meth(N.Application, "set_targetFrameRate", 1);
    N.IVD         = FindClass("", "ItemsVisualData");
    N.tIVD        = TypeOf(N.IVD);
    N.IVD_GetItemDataByID = Meth(N.IVD, "GetItemDataByID", 1);
    N.ivd_items   = Off(N.IVD, "_itemDatas");
    N.tm_activeStage = Off(N.Tutorial, "_activeStage");
    N.MonoCoreLoop   = FindClass("Client.CoreLoop", "MonoCoreLoop");
    N.tMonoCoreLoop  = TypeOf(N.MonoCoreLoop);
    N.InitState      = FindClass("Client.CoreLoop", "InitState");
    N.mcl_sm         = Off(N.MonoCoreLoop, "_coreLoopSM");
    N.sm_state       = Off(FindClass("Client.CoreLoop", "CoreLoopStateMachine"), "_currentCoreLoopState");
    N.is_gdpr        = Off(N.InitState, "OnGDRPCallback");
    N.LoadingWindow  = FindClass("Bowling.GUI", "LoadingWindow");
    N.tLoadingWindow = TypeOf(N.LoadingWindow);
    N.lw_conn        = Off(N.LoadingWindow, "_connectionSection");
    N.lw_noConn      = Off(N.LoadingWindow, "_noConnectionSection");
    N.lw_login       = Off(N.LoadingWindow, "_loginSection");
    N.lw_online      = Off(N.LoadingWindow, "_onlineSection");
    N.lw_offline     = Off(N.LoadingWindow, "_offlineSection");
    N.LW_Switch      = Meth(N.LoadingWindow, "SwitchToSection", 1);
    N.GO_activeInHierarchy = Meth(N.GameObject, "get_activeInHierarchy", 0);
    N.Connector  = FindClass("", "Connector");
    N.Reconnect  = FindClass("", "ReconnectManager");
    N.FloatWnd   = FindClass("", "FloatinMenuWnd");
    N.tFloatWnd  = TypeOf(N.FloatWnd);
    N.GameLogger = FindClass("", "Logger");
    N.Processing = FindClass("", "Processing");
    N.OilGen      = FindClass("", "OilMapGenerator");
    N.tOilGen     = TypeOf(N.OilGen);
    N.OilDescData = FindClass("", "OilDescriptionData");
    N.tOilDescData = TypeOf(N.OilDescData);
    N.Drawer      = FindClass("Kegel", "Drawer");
    N.PracticeMgr = FindClass("Managers", "PracticeManager");
    N.OG_reloadCurrent = Meth(N.OilGen, "ReloadCurrentOilMap", 0);
    N.OG_reloadRedraw  = Meth(N.OilGen, "ReloadAndRedrawOil", 0);
    N.OG_showOnLane    = Meth(N.OilGen, "ShowOilOnLane", 1);
    N.OG_isActive      = Meth(N.OilGen, "IsActive", 0);
    N.og_lines         = Off(N.OilGen, "Lines");
    N.OG_texForId      = Meth(N.OilGen, "GetTextureForId", 1);
    N.OG_reloadById    = Meth(N.OilGen, "ReloadOilMap", 1);        // private; redraws one pattern's cached picture
    N.OG_colorData     = Meth(N.OilGen, "get_ColorData", 0);       // private static: the OilColorData asset
    N.OG_genRG16       = Meth(N.OilGen, "GenerateRG16Texture", 2);
    N.OG_updateColor   = Meth(N.OilGen, "UpdateOilColor", 0);      // private: pushes the game's own oil hue
    if (Il2CppClass *grad = FindClass("UnityEngine", "Gradient")) {
        N.G_getKeys = Meth(grad, "get_colorKeys", 0);
        N.G_setKeys = Meth(grad, "set_colorKeys", 1);
    }
    N.Replayer         = FindClass("", "ReplayerInterfaceManager");
    N.tReplayer        = TypeOf(N.Replayer);
    N.ri_oilTex        = Off(N.Replayer, "oilTexture");
    N.D_drawFromFile   = Meth(N.Drawer, "drawFromFile", 1);
    N.D_graphF         = Meth(N.Drawer, "Graph3DForward", 0);
    N.D_graphR         = Meth(N.Drawer, "Graph3DReverse", 0);
    N.R_getSharedMat   = Meth(N.Renderer, "get_sharedMaterial", 0);
    N.M_hasProp        = Meth(N.Material, "HasProperty", 1, "System.String");
    N.M_getFloatI      = Meth(N.Material, "GetFloat", 1, "System.Int32");
    N.M_setFloatS      = Meth(N.Material, "SetFloat", 2, "System.String");
    N.M_getTexI        = Meth(N.Material, "GetTexture", 1, "System.Int32");
    Il2CppClass *shader = FindClass("UnityEngine", "Shader"), *texture = FindClass("UnityEngine", "Texture");
    N.Sh_propToId      = Meth(shader, "PropertyToID", 1, "System.String");
    N.T_getWrap        = Meth(texture, "get_wrapMode", 0);
    N.T_setWrap        = Meth(texture, "set_wrapMode", 1);
    N.Proc_Hide  = Meth(N.Processing, "Hide", 0);
    N.proc_count = Off(N.Processing, "_spinerCount");
    N.App_reach  = Meth(N.Application, "get_internetReachability", 0);

    N.tRunPsycsTest = TypeOf(N.RunPsycsTest);
    N.tPinHolder    = TypeOf(N.PinHolder);
    N.tRigidbody    = TypeOf(N.Rigidbody);
    N.tRenderer     = TypeOf(N.Renderer);
    N.tLoadDLCTex   = TypeOf(N.LoadDLCTex);
    N.tArsenal      = TypeOf(N.Arsenal);
    N.tTexture2D    = TypeOf(N.Texture2D);
    N.tAssetBundle  = TypeOf(N.AssetBundle);
    N.tTutorial     = TypeOf(N.Tutorial);

    N.FindObjectsOfType      = Meth(N.UObject, "FindObjectsOfType", 1, "System.Type");
    N.FindObjectsOfTypeAll   = Meth(N.UObject, "FindObjectsOfTypeAll", 1, "System.Type");
    N.GetName                = Meth(N.UObject, "get_name", 0);
    N.SetHideFlags           = Meth(N.UObject, "set_hideFlags", 1);
    N.GetGameObject          = Meth(N.Component, "get_gameObject", 0);
    N.GetComponent           = Meth(N.GameObject, "GetComponent", 1, "System.Type");
    N.GetComponentInChildren = Meth(N.GameObject, "GetComponentInChildren", 2, "System.Type", "System.Boolean");
    N.RB_isKinematic = Meth(N.Rigidbody, "get_isKinematic", 0);
    N.RB_getCDM      = Meth(N.Rigidbody, "get_collisionDetectionMode", 0);
    N.RB_setCDM      = Meth(N.Rigidbody, "set_collisionDetectionMode", 1);
    N.RB_getVel      = Meth(N.Rigidbody, "get_linearVelocity", 0);
    N.RB_setVel      = Meth(N.Rigidbody, "set_linearVelocity", 1);
    N.RB_getAngVel   = Meth(N.Rigidbody, "get_angularVelocity", 0);
    N.RB_setAngVel   = Meth(N.Rigidbody, "set_angularVelocity", 1);
    N.RB_getMaxAng   = Meth(N.Rigidbody, "get_maxAngularVelocity", 0);
    N.rpt_rpmText    = Off(N.RunPsycsTest, "rpm");
    if (Il2CppClass *tx = FindClass("UnityEngine.UI", "Text")) { N.Txt_get = Meth(tx, "get_text", 0); N.Txt_set = Meth(tx, "set_text", 1); }
    N.RB_setMaxAngVel = Meth(N.Rigidbody, "set_maxAngularVelocity", 1);
    N.RB_getLinDamp  = Meth(N.Rigidbody, "get_linearDamping", 0);
    N.RB_getAngDamp  = Meth(N.Rigidbody, "get_angularDamping", 0);
    N.R_getMaterial      = Meth(N.Renderer, "get_material", 0);
    N.M_getMainTex       = Meth(N.Material, "get_mainTexture", 0);
    N.M_setMainTex       = Meth(N.Material, "set_mainTexture", 1);
    N.Vec3Cls = FindClass("UnityEngine", "Vector3");
    if (Il2CppClass *k = FindClass("UnityEngine", "Screen")) { N.Scr_w = Meth(k, "get_width", 0); N.Scr_h = Meth(k, "get_height", 0); }
    if (Il2CppClass *k = FindClass("UnityEngine", "Canvas")) {
        N.Cv_mode = Meth(k, "get_renderMode", 0); N.Cv_cam = Meth(k, "get_worldCamera", 0); N.Cv_root = Meth(k, "get_rootCanvas", 0);
        N.tCanvas = TypeOf(k);
    }
    if (Il2CppClass *k = FindClass("UnityEngine", "RectTransform")) N.RT_corners = Meth(k, "GetWorldCorners", 1);
    if (Il2CppClass *k = FindClass("UnityEngine", "Camera")) {
        N.Cam_w2s = Meth(k, "WorldToScreenPoint", 1, "UnityEngine.Vector3"); N.Cam_all = Meth(k, "get_allCameras", 0); N.Cam_depth = Meth(k, "get_depth", 0);
        N.Cam_mask = Meth(k, "get_cullingMask", 0); N.Cam_target = Meth(k, "get_targetTexture", 0); N.Cam_main = Meth(k, "get_main", 0);
    }
    if (Il2CppClass *k = FindClass("UnityEngine", "Behaviour")) N.Beh_enabled = Meth(k, "get_enabled", 0);
    N.GO_activeH = Meth(N.GameObject, "get_activeInHierarchy", 0);
    N.GO_layer = Meth(N.GameObject, "get_layer", 0);
    N.GO_inParent = Meth(N.GameObject, "GetComponentInParent", 1, "System.Type");
    if (Il2CppClass *k = FindClass("UnityEngine", "Collider")) N.Col_bounds = Meth(k, "get_bounds", 0);
    if (Il2CppClass *k = FindClass("UnityEngine", "BoxCollider")) N.tBoxCollider = TypeOf(k);
    if (Il2CppClass *k = FindClass("Managers.Bowling", "MainMenuButtonMan")) {        // the in-game top-right pin layout
        N.tMMBM = TypeOf(k);
        N.mmbm_pinObj = Off(k, "pinObj");
        N.mmbm_pinBack = Off(k, "pinObjBack");
    }
    if (Il2CppClass *k = FindClass("", "RenderToTextureOnce")) N.RTO_render = Meth(k, "renderOnce", 0);   // refreshes the little screen under the ball return
    N.rpt_renderMon = Off(N.RunPsycsTest, "renderToTexMonitor");
    if (Il2CppClass *ta = FindClass("UnityEngine", "TextAsset")) N.TA_getText = Meth(ta, "get_text", 0);   // the built-in patterns' Kegel files
    N.InvData            = FindClass("", "InventaryData");     // holds every pin model's material
    N.tInvData           = N.InvData ? TypeOf(N.InvData) : nullptr;
    N.inv_pinMat         = N.InvData ? Off(N.InvData, "pinMaterial") : -1;
    N.inv_mirrorMat      = N.InvData ? Off(N.InvData, "mirrorPinMaterial") : -1;
    N.inv_rpmFactor      = N.InvData ? Off(N.InvData, "rpmFactor") : -1;       // spin -> grip (BallSoundManager)
    N.inv_maxOmega       = N.InvData ? Off(N.InvData, "_ballMaxOmega") : -1;   // the spin the grip formula caps at
    N.inv_maxRpm         = N.InvData ? Off(N.InvData, "ballMaxRmp") : -1;      // _ballMaxOmega = this x 2 pi / 60, filled on first use
    N.T2D_ctor        = Meth(N.Texture2D, ".ctor", 2, "System.Int32", "System.Int32");
    N.LoadImage       = Meth(N.ImageConversion, "LoadImage", 2, "UnityEngine.Texture2D", "System.Byte[]");
    N.AB_LoadFromFile = Meth(N.AssetBundle, "LoadFromFile", 1, "System.String");
    N.AB_LoadAsset    = Meth(N.AssetBundle, "LoadAsset", 2, "System.String", "System.Type");
    N.AB_Unload       = Meth(N.AssetBundle, "Unload", 1, "System.Boolean");
    N.RPT_UpdatePinPositions = Meth(N.RunPsycsTest, "UpdatePinPositions", 0);
    N.LDT_textureLoaded  = Meth(N.LoadDLCTex, "textureLoaded", 1);
    N.LDT_loadForItem    = Meth(N.LoadDLCTex, "loadTextureForItem", 3, "Models.mdl_Shop_Item");
    N.InvD_InitBallsOnReturner = Meth(N.InventaryData, "InitBallsOnReturner", 1);
    N.TM_IsInTutorial    = Meth(N.Tutorial, "IsInTutorial", 0);
    N.TM_SkipTutorial    = Meth(N.Tutorial, "SkipTutorial", 0);
    N.CSM_TutorialDone   = Meth(N.CSM, "get_IsTutorialCompleted", 0);
    N.IH_GetShopItemById = Meth(N.ItemHolder, "GetShopItemById", 1, "System.Int32");
    N.SI_getDLC          = Meth(N.ShopItem, "getDLC", 2);
    N.Ars_InitScrollData = Meth(N.Arsenal, "InitScrollData", 0);

    N.rpt_sphere       = Off(N.RunPsycsTest, "sphere");
    N.rpt_sphereRender = Off(N.RunPsycsTest, "sphere_render");
    N.rpt_kegsUp       = Off(N.RunPsycsTest, "_kegsUp");
    N.rpt_kegsUpdate   = Off(N.RunPsycsTest, "kegsUpdate");
    N.ph_pins       = Off(N.PinHolder, "_pins");
    N.pin_physic    = Off(N.Pin, "_physic");
    N.pin_visual    = Off(N.Pin, "_objVisualRoot");
    N.pin_hq        = Off(N.Pin, "_highQualityRender");
    N.pin_lq        = Off(N.Pin, "_lowQualityRender");
    N.pin_mirror    = Off(N.Pin, "_mirrorRenderer");
    N.Comp_getTransform = Meth(N.Component, "get_transform", 0);
    N.GO_getTransform   = Meth(N.GameObject, "get_transform", 0);
    if (Il2CppClass *tr = FindClass("UnityEngine", "Transform")) {
        N.Tr_getRot      = Meth(tr, "get_rotation", 0);
        N.Tr_setRot      = Meth(tr, "set_rotation", 1);
        N.Tr_getLocalRot = Meth(tr, "get_localRotation", 0);
        N.Tr_setLocalRot = Meth(tr, "set_localRotation", 1);
    }
    N.ldt_dlcID     = Off(N.LoadDLCTex, "dlcIDToLoad");
    N.ldt_toChange  = Off(N.LoadDLCTex, "toChangeTexture");
    N.ldt_objectID  = Off(N.LoadDLCTex, "objectID");
    N.si_dlcLink    = Off(N.ShopItem, "dlc_link");
    N.ih_shop       = Off(N.ItemHolder, "_shop");
    N.item_itemId   = Off(N.Item, "item_id");
    N.item_baseId   = Off(N.Item, "base_ide");
    N.shop_name     = Off(N.ShopItem, "itm_name");
    N.ars_ballsData = Off(N.Arsenal, "_ballsData");
    N.ars_scroll    = Off(N.Arsenal, "_ballsScroll");

    // Static fields (GameParams etc.) are read later, once the lane is up: reading one can
    // run that class's start-up code, and we never want to run it before the game does.
    if (!N.FindObjectsOfType || !N.GetComponent || !N.GetGameObject) return false;
    BFLog(@"connected to the game");
    return true;
}

// Reads a static object field directly - no game getters, so no side effects
// (e.g. ItemHolder.Instance CREATES the item database if it doesn't exist yet).
static void *ReadStaticObj(FieldInfo *&f, Il2CppClass *k, const char *name) {
    if (!k) return nullptr;
    if (!f) f = StaticField(k, name);
    void *v = nullptr;
    if (f) StaticRead(f, &v);
    return v;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void *Elem(Il2CppArray *a, size_t i) { return ((void **)Data(a))[i]; }

static Il2CppArray *FindAll(Il2CppObject *type) {            // active scene objects
    if (!type || !N.FindObjectsOfType) return nullptr;
    void *a[] = { type };
    return (Il2CppArray *)Invoke(N.FindObjectsOfType, nullptr, a);
}

static Il2CppArray *FindAllLoaded(Il2CppObject *type) {      // everything in memory, incl. assets
    if (!type || !N.FindObjectsOfTypeAll) return nullptr;
    void *a[] = { type };
    return (Il2CppArray *)Invoke(N.FindObjectsOfTypeAll, nullptr, a);
}

static void *FirstAlive(Il2CppArray *a) {
    for (size_t i = 0; i < Len(a); i++) {
        void *o = Elem(a, i);
        if (Alive(o)) return o;
    }
    return nullptr;
}

static void *GameObjectOf(void *component) {
    return Alive(component) ? (void *)Invoke(N.GetGameObject, component, nullptr) : nullptr;
}

static void *GetComp(void *go, Il2CppObject *type) {
    if (!Alive(go) || !type) return nullptr;
    void *a[] = { type };
    void *c = Invoke(N.GetComponent, go, a);
    return Alive(c) ? c : nullptr;
}

static void *GetCompInChildren(void *go, Il2CppObject *type) {
    if (!Alive(go) || !type || !N.GetComponentInChildren) return nullptr;
    bool includeInactive = true;
    void *a[] = { type, &includeInactive };
    void *c = Invoke(N.GetComponentInChildren, go, a);
    return Alive(c) ? c : nullptr;
}

static NSString *NameOf(void *unityObject) {
    return (Alive(unityObject) && N.GetName) ? Str(Invoke(N.GetName, unityObject, nullptr)) : nil;
}

static void DontUnload(void *unityObject) {   // HideFlags.DontUnloadUnusedAsset
    if (!N.SetHideFlags || !Alive(unityObject)) return;
    int flags = 32;
    void *a[] = { &flags };
    Invoke(N.SetHideFlags, unityObject, a);
}

static NSString *Squash(NSString *s) {        // "Match-Up BP" -> "matchupbp"
    NSMutableString *o = [NSMutableString stringWithCapacity:s.length];
    for (NSUInteger i = 0; i < s.length; i++) {
        unichar c = [s characterAtIndex:i];
        if (c < 128 && isalnum((int)c)) [o appendFormat:@"%c", (char)tolower((int)c)];
    }
    return o;
}

struct ListView { void **items; int size; int sizeOff; int versionOff; int version; };

static bool ReadList(void *list, ListView &lv) {   // System.Collections.Generic.List<T>
    if (!list) return false;
    Il2CppClass *k = ClassOf(list);
    int itemsOff = FieldOffset(k, "_items");
    lv.sizeOff = FieldOffset(k, "_size");
    lv.versionOff = FieldOffset(k, "_version");
    if (itemsOff < 0 || lv.sizeOff < 0) return false;
    Il2CppArray *arr = At<Il2CppArray *>(list, itemsOff);
    int size = At<int>(list, lv.sizeOff);
    if (!arr || size < 0 || (size_t)size > Len(arr)) return false;
    lv.items = (void **)Data(arr);
    lv.size = size;
    lv.version = lv.versionOff >= 0 ? At<int>(list, lv.versionOff) : 0;
    return true;
}

// Holds a game object safely between frames (GC handle + "still exists" check)
struct Ref {
    GCHandle h = 0;   // pointer-sized! (see Il2Cpp.h)
    void *get() {
        if (!h) return nullptr;
        void *o = Target(h);
        if (!Alive(o)) { Release(h); h = 0; return nullptr; }
        return o;
    }
    void set(void *o) {
        if (h) { Release(h); h = 0; }
        if (o) h = Keep(o);
    }
};

// Same, for plain C# objects (store entries etc.) that have no Unity "still exists" flag
struct ObjRef {
    GCHandle h = 0;
    void *get() { return h ? Target(h) : nullptr; }
    void set(void *o) {
        if (h) { Release(h); h = 0; }
        if (o) h = Keep(o);
    }
};

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
static int sFrame = 0, sLoc = -1, sPrevLoc = -1, sMode = -1;
static Ref sRPT;                        // RunPsycsTest: the game's main lane/throw controller
static CFAbsoluteTime sRPTSince = 0;    // when the lane controller showed up
static bool sSettled = false;           // lane up for a few seconds: safe to use game data
static bool sHealthy = false;

static void NotReady() {
    sSettled = false;
    sMode = sLoc = -1;
    gBFStatus.offline = false;
    gBFStatus.gameMode = gBFStatus.location = -1;
}

static void UpdateState() {
    void *rpt = sRPT.get();
    if (!rpt && sFrame % 30 == 0) {
        rpt = FirstAlive(FindAll(N.tRunPsycsTest));
        sRPT.set(rpt);
        if (rpt) { sRPTSince = CFAbsoluteTimeGetCurrent(); BFLog(@"lane found, waiting for it to settle"); }
    }
    if (!rpt) { NotReady(); return; }
    CFAbsoluteTime up = CFAbsoluteTimeGetCurrent() - sRPTSince;
    if (up < 4.0) { NotReady(); return; }   // let the game finish loading before touching anything
    if (!N.gp_gameMode || !N.gp_location) {
        N.gp_gameMode = StaticField(N.GameParams, "gameMode");
        N.gp_location = StaticField(N.GameParams, "_playerLocation");
        if (!N.gp_gameMode || !N.gp_location) { NotReady(); return; }
    }
    int mode = -1, loc = -1;
    StaticRead(N.gp_gameMode, &mode);
    StaticRead(N.gp_location, &loc);
    sMode = mode;
    sLoc = loc;
    sSettled = true;
    gBFStatus.gameMode = mode;
    gBFStatus.location = loc;
    gBFStatus.offline = mode == MODE_FUN && loc != LOC_REPLAYER;
    if (!sHealthy && up > 14.0) { sHealthy = true; BFMarkHealthy(); }
}

// ---------------------------------------------------------------------------
// Pin physics fix
// All 10 pins ship with "Discrete" collision detection, so a fast flying pin can move
// several cm between physics steps and pass straight through another pin's thin neck.
// "Continuous Dynamic" checks the whole path between steps. The game copies this
// setting whenever it rebuilds a pin, so it sticks.
// ---------------------------------------------------------------------------
static Ref sHolders[4];
static bool sPinFixOn = false, sBallCCDOn = false;

static void SetCDM(void *rb, int mode) {
    if (!rb || !N.RB_getCDM || !N.RB_setCDM) return;
    // Unity only allows continuous modes on bodies that are moved by physics
    if (mode != CDM_DISCRETE && InvokeBool(N.RB_isKinematic, rb, nullptr, true)) return;
    int cur = InvokeInt(N.RB_getCDM, rb, nullptr, -1);
    if (cur < 0 || cur == mode) return;
    void *a[] = { &mode };
    Invoke(N.RB_setCDM, rb, a);
}

// Pins also get a higher spin limit. They ran on Unity's old default (7 rad/s, newer Unity
// uses 50). A pin clipped low at the base has to spin faster than that to tip over, so the
// extra energy was thrown away and the pin just rocked in place.
static void SetPinPhysics(void *rb, bool on) {
    if (!rb || !N.RB_getCDM || !N.RB_setCDM) return;
    if (on && InvokeBool(N.RB_isKinematic, rb, nullptr, true)) return;
    int mode = on ? PinCDM() : CDM_DISCRETE;
    int cur = InvokeInt(N.RB_getCDM, rb, nullptr, -1);
    if (cur < 0 || cur == mode) return;
    void *a[] = { &mode };
    Invoke(N.RB_setCDM, rb, a);
    if (N.RB_setMaxAngVel) {
        float w = on ? 50.0f : 7.0f;
        void *b[] = { &w };
        Invoke(N.RB_setMaxAngVel, rb, b);
    }
}

static void *BallBody() {
    void *rpt = sRPT.get();
    if (!rpt || N.rpt_sphere < 0) return nullptr;
    return GetComp(At<void *>(rpt, N.rpt_sphere), N.tRigidbody);
}

static void PinFixTick() {
    bool want = gBF.pinFix && gBFStatus.offline;
    if ((!want && !sPinFixOn) || sFrame % 15 != 0) return;
    if (!N.PinHolder || N.ph_pins < 0 || N.pin_physic < 0) return;
    if (sFrame % 120 == 0 || !sHolders[0].get()) {
        Il2CppArray *all = FindAll(N.tPinHolder);
        for (int i = 0; i < 4; i++) {
            void *h = (size_t)i < Len(all) ? Elem(all, i) : nullptr;
            sHolders[i].set(Alive(h) ? h : nullptr);
        }
    }
    for (int i = 0; i < 4; i++) {
        void *holder = sHolders[i].get();
        ListView lv;
        if (!holder || !ReadList(At<void *>(holder, N.ph_pins), lv)) continue;
        for (int p = 0; p < lv.size; p++) {
            void *pin = lv.items[p];
            if (!pin) continue;
            SetPinPhysics(GetComp(GameObjectOf(At<void *>(pin, N.pin_physic)), N.tRigidbody), want);
        }
    }
    sPinFixOn = want;
}

// ---- your own pin image (looks only) ----
// Every pin (all lanes) shares one material, "pin_diff" (Legacy Shaders/Reflective/Diffuse), and the
// lane reflection uses "keg_d"; both show the pin image as _MainTex (the game's own is "pins1", 512 x
// 512, from the pins/fulltextures bundle; Pin Arsenal designs swap it). The player's square PNG is
// loaded into a Texture2D and set as those materials' main texture; whatever the game had is kept per
// material and put back when the switch goes off. If the game changes pins (Pin Arsenal), the new
// texture becomes the one to put back and the player's image goes on again.
// Besides the 10 real pins, the scene has more pin models: the ones the pinsetter carries down for a new
// rack (pinspotter joints), the pin deck, reflections and the Arsenal display. They all share
// InventaryData.pinMaterial / mirrorPinMaterial (InventaryData.ApplyTexturePins assigns them), so those
// two get the image too; otherwise every new rack came down with the game's pins and then popped.
// Materials already changed are re-checked every frame, so a reset by the game never shows.
NSString *BFPinImagePath(void) {
    NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    return [[docs stringByAppendingPathComponent:@"BowlingPlus"] stringByAppendingPathComponent:@"pin_image.png"];
}

static uintptr_t sPinImgTex = 0;
static NSDate *sPinImgStamp;
static int sPinImgSize = 0, sPinImgFails = 0, sPinImgMats = 0;
static const int kPinMatMax = 16;
static Ref sInvData;
static ObjRef sPinMat[kPinMatMax];                // materials we changed
static uintptr_t sPinMatOrig[kPinMatMax];         // what each one showed before (strong handles)
static int sPinMatCount = 0;

static void *PinImageTexture() {
    NSString *path = BFPinImagePath();
    NSDate *stamp = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:nil].fileModificationDate;
    void *t = sPinImgTex ? Target(sPinImgTex) : nullptr;
    if (Alive(t) && stamp && [stamp isEqualToDate:sPinImgStamp]) return t;
    if (!stamp || sPinImgFails >= 3 || !N.Texture2D || !N.T2D_ctor || !N.LoadImage || !N.Byte) return nullptr;
    NSData *png = [NSData dataWithContentsOfFile:path];
    if (!png.length) return nullptr;
    Il2CppObject *tex = NewObject(N.Texture2D);
    int w = 2, h = 2;
    bool ok = false;
    void *ca[] = { &w, &h };
    if (tex) Invoke(N.T2D_ctor, tex, ca, &ok);
    Il2CppArray *bytes = ok ? NewArray(N.Byte, png.length) : nullptr;
    if (bytes) memcpy(Data(bytes), png.bytes, png.length);
    void *la[] = { tex, bytes };
    if (!bytes || !InvokeBool(N.LoadImage, nullptr, la, false)) { sPinImgFails++; BFLog(@"pin image: couldn't load it"); return nullptr; }
    DontUnload(tex);
    if (sPinImgTex) Release(sPinImgTex);
    sPinImgTex = Keep(tex);
    sPinImgStamp = stamp;
    sPinImgFails = 0;
    UIImage *ui = [UIImage imageWithData:png];
    sPinImgSize = (int)ui.size.width;
    BFLog(@"pin image loaded (%d px)", sPinImgSize);
    return tex;
}

static int PinMatIndex(void *mat) {
    for (int i = 0; i < sPinMatCount; i++) if (sPinMat[i].get() == mat) return i;
    return -1;
}

static void PinImageOn(void *mat, void *tex) {
    if (!Alive(mat)) return;
    void *cur = (void *)Invoke(N.M_getMainTex, mat, nullptr);
    if (cur == tex) return;
    int i = PinMatIndex(mat);
    if (i < 0) {
        if (sPinMatCount >= kPinMatMax) return;
        i = sPinMatCount++;
        sPinMat[i].set(mat);
        sPinMatOrig[i] = 0;
    }
    if (sPinMatOrig[i]) Release(sPinMatOrig[i]);  // the game's texture (it may have just changed pins)
    sPinMatOrig[i] = Alive(cur) ? Keep(cur) : 0;
    void *a[] = { tex };
    Invoke(N.M_setMainTex, mat, a);
}

static void PinImageRestore() {
    void *mine = sPinImgTex ? Target(sPinImgTex) : nullptr;
    for (int i = 0; i < sPinMatCount; i++) {
        void *mat = sPinMat[i].get();
        void *orig = sPinMatOrig[i] ? Target(sPinMatOrig[i]) : nullptr;
        if (Alive(mat) && Alive(orig) && (void *)Invoke(N.M_getMainTex, mat, nullptr) == mine) {
            void *a[] = { orig };
            Invoke(N.M_setMainTex, mat, a);
        }
        if (sPinMatOrig[i]) Release(sPinMatOrig[i]);
        sPinMatOrig[i] = 0;
        sPinMat[i].set(nullptr);
    }
    sPinMatCount = 0;
}

static void PinImageTick() {
    if (!sSettled || !N.M_getMainTex || !N.M_setMainTex || !N.R_getSharedMat) return;
    bool full = sFrame % 30 == 0;
    void *tex = nullptr;
    if (gBF.pinImage) tex = full ? PinImageTexture() : (sPinImgTex ? Target(sPinImgTex) : nullptr);
    if (!Alive(tex)) {
        if (full && sPinMatCount) { PinImageRestore(); BFLog(@"pin image off: the game's pins are back"); }
        if (full) sPinImgMats = 0;
        return;
    }
    for (int i = 0; i < sPinMatCount; i++) {      // every frame: the materials already showing it
        void *mat = sPinMat[i].get();
        if (Alive(mat) && (void *)Invoke(N.M_getMainTex, mat, nullptr) != tex) PinImageOn(mat, tex);
    }
    if (!full) return;
    void *inv = sInvData.get();                   // the pinsetter / pin deck / reflection / Arsenal pins
    if (!inv && N.tInvData && sFrame % 120 == 0) { inv = FirstAlive(FindAll(N.tInvData)); sInvData.set(inv); }
    if (inv) {
        if (N.inv_pinMat >= 0) PinImageOn(At<void *>(inv, N.inv_pinMat), tex);
        if (N.inv_mirrorMat >= 0) PinImageOn(At<void *>(inv, N.inv_mirrorMat), tex);
    }
    if (N.PinHolder && N.ph_pins >= 0) {          // the real pins
        for (int i = 0; i < 4; i++) {
            void *holder = sHolders[i].get();
            ListView lv;
            if (!holder || !ReadList(At<void *>(holder, N.ph_pins), lv)) continue;
            for (int p = 0; p < lv.size; p++) {
                void *pin = lv.items[p];
                if (!Alive(pin)) continue;
                int offs[3] = { N.pin_hq, N.pin_lq, N.pin_mirror };
                for (int k = 0; k < 3; k++) {
                    void *r = offs[k] >= 0 ? At<void *>(pin, offs[k]) : nullptr;
                    if (!Alive(r)) continue;
                    void *mat = (void *)Invoke(N.R_getSharedMat, r, nullptr);
                    if (Alive(mat)) PinImageOn(mat, tex);
                }
            }
        }
    }
    sPinImgMats = sPinMatCount;
}

NSString *BFPinImageStatus(void) {
    BOOL have = [[NSFileManager defaultManager] fileExistsAtPath:BFPinImagePath()];
    if (!have) return @"No image yet. Get the wrap template, draw your design on it (2:1), then pick it.";
    if (!gBF.pinImage) return @"Your image is saved. Turn the switch on to use it.";
    if (sPinImgFails >= 3) return @"Couldn't load that image. Try picking it again (PNG or JPEG).";
    if (sPinImgMats > 0) return [NSString stringWithFormat:@"On the pins now (%d px image).", sPinImgSize];
    return @"Shows on the pins when a lane is on screen.";
}

static void BallCCDTick() {   // the ball too, while the pin fix or a speed boost is on
    if (sFrame % 15 != 0) return;
    bool want = gBFStatus.offline && (gBF.pinFix || gBF.speedMult > 1.5f);
    if (!want && !sBallCCDOn) return;
    void *rb = BallBody();
    if (!rb) return;
    SetCDM(rb, want ? BallCDM() : CDM_DISCRETE);
    sBallCCDOn = want;
}

// ---------------------------------------------------------------------------
// Ball speed (Practice only)
// The game launches the ball with one push, then the hook comes from friction with
// the oil. So we wait for the launch and multiply the ball's speed once.
// ---------------------------------------------------------------------------
static bool sBoosted = false;

static void SpeedTick() {
    float mult = fminf(gBF.speedMult, BF_MAX_SPEED);
    if (!gBFStatus.offline || mult < 1.01f || sLoc != LOC_THROWING) { sBoosted = false; return; }
    if (sBoosted || !N.RB_getVel || !N.RB_setVel) return;
    void *rb = BallBody();
    if (!rb || InvokeBool(N.RB_isKinematic, rb, nullptr, true)) return;
    bool ok = false;
    Il2CppObject *boxed = Invoke(N.RB_getVel, rb, nullptr, &ok);
    if (!ok || !boxed) return;
    Vec3 v = *(Vec3 *)Unbox(boxed);
    float speed = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (speed < 1.0f) return;   // not launched yet
    if (mult > 1.5f) SetCDM(rb, BallCDM());   // a very fast ball must not skip through the pins
    Vec3 nv = { v.x * mult, v.y * mult, v.z * mult };
    void *a[] = { &nv };
    Invoke(N.RB_setVel, rb, a);
    sBoosted = true;
    BFLog(@"ball speed x%.1f (%.1f -> %.1f m/s)", mult, speed, speed * mult);
}

// ---------------------------------------------------------------------------
// Spin (RPM) boost (Practice only)
// At release the game turns the throw's spin (RunPsycsTest.rotation, in rpm, capped near 600 by the
// controls) into the ball's spin once: AddTorque(rotation x 2 pi / 60, VelocityChange). The hook is
// the physics engine's friction, which BallSoundManager.OnCollisionStay sets every step:
//   friction = oil x min(spin, BallMaxOmega) / BallMaxOmega x InventaryData.rpmFactor x (wear)
// so spin above BallMaxOmega adds nothing (1.4.9 spun the ball faster but didn't hook more). The boost
// multiplies the ball's spin once right after release (same axis, same hook side) and, for the spin the
// cap cuts off, raises rpmFactor by the same amount while that ball rolls (exactly the friction an
// uncapped formula would give), then puts the game's value back.
// ---------------------------------------------------------------------------
static bool sSpun = false;
static int sSpunFrom = 0, sSpunTo = 0;
static float sRpmFactorOrig = -1, sGrip = 1;
static Ref sSpinInv;

static void *SpinInventary() {
    void *inv = sSpinInv.get();
    if (!inv && N.tInvData) { inv = FirstAlive(FindAll(N.tInvData)); sSpinInv.set(inv); }
    return inv;
}

static void SpinGripRestore() {
    if (sRpmFactorOrig < 0) return;
    void *inv = SpinInventary();
    if (inv && N.inv_rpmFactor >= 0) At<float>(inv, N.inv_rpmFactor) = sRpmFactorOrig;
    BFLog(@"ball grip back to the game's (rpmFactor %.3f)", sRpmFactorOrig);
    sRpmFactorOrig = -1;
}

static void SpinTick() {
    float mult = fminf(fmaxf(gBF.spinMult, 1.0f), BF_MAX_SPIN);
    if (!gBFStatus.offline || mult < 1.01f || sLoc != LOC_THROWING) { sSpun = false; SpinGripRestore(); return; }
    if (sSpun || !N.RB_getAngVel || !N.RB_setAngVel || !N.RB_getVel) return;
    void *rb = BallBody();
    if (!rb || InvokeBool(N.RB_isKinematic, rb, nullptr, true)) return;
    bool ok = false;
    Il2CppObject *bv = Invoke(N.RB_getVel, rb, nullptr, &ok);
    if (!ok || !bv) return;
    Vec3 v = *(Vec3 *)Unbox(bv);
    if (sqrtf(v.x * v.x + v.y * v.y + v.z * v.z) < 1.0f) return;   // not released yet
    sSpun = true;
    Il2CppObject *bw = Invoke(N.RB_getAngVel, rb, nullptr, &ok);
    if (!ok || !bw) return;
    Vec3 w = *(Vec3 *)Unbox(bw);
    float rad = sqrtf(w.x * w.x + w.y * w.y + w.z * w.z);
    if (rad < 0.5f) return;                       // a no-spin throw: nothing to multiply
    if (N.RB_getMaxAng) {
        Il2CppObject *bm = Invoke(N.RB_getMaxAng, rb, nullptr, &ok);
        float maxw = (ok && bm) ? *(float *)Unbox(bm) : 0;
        if (maxw > 0 && rad * mult > maxw) mult = maxw / rad;   // never past the engine's own limit
    }
    Vec3 nw = { w.x * mult, w.y * mult, w.z * mult };
    void *a[] = { &nw };
    Invoke(N.RB_setAngVel, rb, a);
    // grip: the friction formula caps spin at BallMaxOmega; make up for what it cuts off
    void *inv = SpinInventary();
    sGrip = 1;
    if (inv && N.inv_rpmFactor >= 0 && N.inv_maxOmega >= 0) {
        float maxOmega = At<float>(inv, N.inv_maxOmega);
        if (maxOmega <= 0 && N.inv_maxRpm >= 0) maxOmega = At<float>(inv, N.inv_maxRpm) * 6.2831853f / 60.f;   // like get_BallMaxOmega
        if (maxOmega > 1) {
            sGrip = fmaxf(1.f, rad * mult / maxOmega);
            if (sGrip > 1.001f) {
                if (sRpmFactorOrig < 0) sRpmFactorOrig = At<float>(inv, N.inv_rpmFactor);
                At<float>(inv, N.inv_rpmFactor) = sRpmFactorOrig * sGrip;
            }
        }
    }
    sSpunFrom = (int)lroundf(rad * 60.f / 6.2831853f);
    sSpunTo = (int)lroundf(rad * mult * 60.f / 6.2831853f);
    BFLog(@"ball spin x%.1f (%d -> %d rpm), grip x%.2f", mult, sSpunFrom, sSpunTo, sGrip);
    // the scoreboard's "599 rpm": show the boosted number
    void *rpt = sRPT.get();
    Il2CppArray *texts = (rpt && N.rpt_rpmText >= 0) ? At<Il2CppArray *>(rpt, N.rpt_rpmText) : nullptr;
    for (size_t i = 0; i < Len(texts) && N.Txt_get && N.Txt_set; i++) {
        void *t = Elem(texts, i);
        if (!Alive(t)) continue;
        NSString *cur = Str((Il2CppString *)Invoke(N.Txt_get, t, nullptr));
        int shown = cur.intValue;
        if (shown <= 0) continue;
        NSString *rest = [cur substringFromIndex:[cur rangeOfCharacterFromSet:NSCharacterSet.decimalDigitCharacterSet.invertedSet].location == NSNotFound
                                                 ? cur.length : [cur rangeOfCharacterFromSet:NSCharacterSet.decimalDigitCharacterSet.invertedSet].location];
        void *ta[] = { NewString([NSString stringWithFormat:@"%d%@", (int)lroundf(shown * mult), rest].UTF8String) };
        Invoke(N.Txt_set, t, ta);
    }
}

// ---------------------------------------------------------------------------
// Spare shooting mode (Practice only)
// Every shot, the game builds a 10-slot "pins standing" list (RunPsycsTest._kegsUp)
// and racks it with UpdatePinPositions(). Pins set to false are dropped far below
// the lane, out of play. On a fresh rack we let you edit that list first.
// ---------------------------------------------------------------------------
static bool sSparePending = false;          // a pick was made for this frame and not thrown yet
static uint16_t sSpareMask = BF_ALL_PINS;

static bool ApplyPinMask(uint16_t mask) {
    void *rpt = sRPT.get();
    if (!rpt || N.rpt_kegsUp < 0 || !N.RPT_UpdatePinPositions) return false;
    Il2CppArray *kegs = At<Il2CppArray *>(rpt, N.rpt_kegsUp);
    size_t n = Len(kegs);
    if (!n || n > 32) return false;
    bool *k = (bool *)Data(kegs);
    for (size_t i = 0; i < n; i++) k[i] = i < 10 ? ((mask >> i) & 1) != 0 : true;   // slot i = pin i+1
    Invoke(N.RPT_UpdatePinPositions, rpt, nullptr);
    // same event the game fires after racking, so the pin display updates
    void *ev = N.rpt_kegsUpdate >= 0 ? At<void *>(rpt, N.rpt_kegsUpdate) : nullptr;
    if (ev) {
        const MethodInfo *inv = FindMethod(ClassOf(ev), "Invoke", 1);
        if (inv) { void *a[] = { kegs }; Invoke(inv, ev, a); }
    }
    BFLog(@"spare mode: racked pins mask 0x%03x", mask);
    return true;
}

static void SpareTick() {
    bool oneShotUp = BFMenuPickerVisible() && BFMenuPickerOneShot();
    if (oneShotUp) {                                   // opened by tapping a pin layout: stays while a shot is being set up
        bool setup = sLoc == LOC_START_POS || sLoc == LOC_BALL_RETURNER || sLoc == LOC_BOTTOM_MONITOR;
        if (!gBFStatus.offline || !setup) { BFMenuHidePinPicker(); oneShotUp = false; }
    }
    // a pick made by tapping stays in force until you throw, even with Spare shooting mode off
    bool active = gBFStatus.offline && (gBF.spareMode || sSparePending);
    if (!active) {
        if (BFMenuPickerVisible() && !oneShotUp) BFMenuHidePinPicker();
        sSparePending = false;
        return;
    }
    if (sLoc == LOC_THROWING) sSparePending = false;
    if (sLoc != LOC_START_POS) {
        if (BFMenuPickerVisible() && !oneShotUp) BFMenuHidePinPicker();
        return;
    }
    if (sPrevLoc == LOC_START_POS) return;           // act once, right when the shot gets set up
    void *rpt = sRPT.get();
    if (!rpt || N.rpt_kegsUp < 0) return;
    Il2CppArray *kegs = At<Il2CppArray *>(rpt, N.rpt_kegsUp);
    size_t n = Len(kegs);
    if (!n || n > 32) return;
    bool *k = (bool *)Data(kegs);
    for (size_t i = 0; i < n; i++) if (!k[i]) return;   // not a fresh rack (2nd ball) - leave it alone
    if (sSparePending) {                               // e.g. you switched balls: keep your pick
        if (sSpareMask != BF_ALL_PINS) ApplyPinMask(sSpareMask);
        return;
    }
    if (!gBF.spareMode) return;
    if (gBF.spareAuto) {                               // auto-rack: same pins every frame, no questions
        BFApplySpareSelection(gBF.lastPinMask);
        return;
    }
    BFMenuShowPinPicker(gBF.lastPinMask);
}

void BFApplySpareSelection(uint16_t mask) {
    mask &= BF_ALL_PINS;
    if (!mask) mask = BF_ALL_PINS;
    sSpareMask = mask;
    sSparePending = true;
    if (mask != BF_ALL_PINS) {
        gBF.lastPinMask = mask;
        BFSaveConfig();
        ApplyPinMask(mask);
    }
}

// Closed the picker with the X: no changes. In Spare mode that also means "don't ask again this frame".
void BFSpareDismissed(void) {
    if (BFMenuPickerOneShot()) return;
    sSpareMask = BF_ALL_PINS;
    sSparePending = true;
}

// Tapped a pin layout: re-rack exactly these pins right now (even all ten), and keep them if the game re-racks
// before you throw (switching balls). Only for this shot: Spare shooting mode stays off.
void BFApplySpareSelectionNow(uint16_t mask) {
    mask &= BF_ALL_PINS;
    if (!mask) mask = BF_ALL_PINS;
    sSpareMask = mask;
    sSparePending = true;
    if (mask != BF_ALL_PINS) { gBF.lastPinMask = mask; BFSaveConfig(); }
    ApplyPinMask(mask);
    void *rpt = sRPT.get();                            // the little screen under the ball return shows a picture of the pins
    void *rm = (rpt && N.rpt_renderMon >= 0) ? At<void *>(rpt, N.rpt_renderMon) : nullptr;
    if (Alive(rm) && N.RTO_render) Invoke(N.RTO_render, rm, nullptr);
}

// ---------------------------------------------------------------------------
// Match Up skins (v1.1.1)
// How the game picks a ball's 3D skin: the ball's store entry has a list of "DLC" ids
// (dlc_link). getDLC(TEXTURE) returns the first one the skin catalog lists as a texture.
// That catalog record's dlc_url is the texture's path inside the game's own asset bundles.
// Most skin files hold two balls side by side, and the store entry's texc flag picks the half.
//
// What's wrong: the Match Up Pearl's store entry points at the wrong skin (the same black look
// as the BP). Its real art (burgundy/black/silver) is the left half of
// Text_MatchupPearl_MatchupHybrid, the file the Match Up Hybrid uses. So we point the Pearl at
// that file (after checking the file's path) and pick the half the Hybrid doesn't use. Then the
// game's own code draws it right everywhere: rack, Arsenal, previews, your hand, replays.
// Match Up BP is left alone when it has a skin of its own. Only if it has none, it gets the
// black art the Pearl had (or, as a last resort, a drawn look-alike on the ball in your hand).
// ---------------------------------------------------------------------------
enum Skin { SKIN_NONE = 0, SKIN_PEARL = 1, SKIN_BP = 2 };
static const char *kPearlAsset = "assets/bundledata/art/balls/fulltextures/text_matchuppearl_matchuphybrid.png";
static NSString *const kPearlTexName = @"Text_MatchupPearl_MatchupHybrid";
static NSString *const kPearlSheetKey = @"matchuppearl";   // in the Pearl/Hybrid file's name
static const int kKnownBlackPearlSkin = 770;               // tex_blackpearl_flameturquoise.png in 1.907 (name is checked first)

enum { PF_OK = 0, PF_WAITING, PF_NO_PEARL, PF_NO_SHEET, PF_LINK_FAILED, PF_OFF };

struct Link {                 // our change to one store entry, so it can be undone
    bool changed = false;
    int origDlc = -1, origTexc = -1;
    int removed[4] = { -1, -1, -1, -1 };
    int nRemoved = 0, added = -1;
};

static ObjRef sPearlShop, sHybridShop, sBPShop, sSolidShop;   // store entries (plain C# objects)
static Link sPearlLink, sBPLink;
static void *sScannedList = nullptr;             // the store list we last looked at
static int sScannedCount = -1;
static bool sPearlFixed = false, sBPFixed = false, sBPHasSkin = false;
static int sPearlFail = PF_WAITING, sPearlSheet = -1, sGamePearlDlc = -1, sGamePearlTexc = -1;
static bool sNeedRackRefresh = false, sNeedHandReload = false;

static GCHandle sPearlTex = 0, sBPTex = 0;       // last-resort textures (ball in your hand only)
static int sPearlTries = 0, sBPTries = 0;
static CFAbsoluteTime sPearlNextTry = 0;

static NSMutableDictionary<NSNumber *, NSString *> *sNames;
static NSString *sBallLine = @"";
static void *sLastItem = nullptr;
static Skin sCurSkin = SKIN_NONE;

static Skin SkinForName(NSString *name) {
    NSString *n = Squash(name ?: @"");
    if (![n hasPrefix:@"matchup"]) return SKIN_NONE;
    if ([n isEqualToString:@"matchuppearl"]) return SKIN_PEARL;
    if ([n containsString:@"black"] || [n hasSuffix:@"bp"]) return SKIN_BP;
    return SKIN_NONE;
}

static void *FindShopItem(int shopId) {
    if (!N.IH_GetShopItemById || shopId <= 0) return nullptr;
    void *holder = ReadStaticObj(N.ih_instance, N.ItemHolder, "_instance");   // never create it ourselves
    if (!holder) return nullptr;
    void *a[] = { &shopId };
    return Invoke(N.IH_GetShopItemById, holder, a);
}

static void *ShopItemOf(void *item) {        // inventory item -> store entry (item_id, then base_ide)
    if (!item) return nullptr;
    int ids[2] = { N.item_itemId >= 0 ? At<int>(item, N.item_itemId) : -1,
                   N.item_baseId >= 0 ? At<int>(item, N.item_baseId) : -1 };
    for (int i = 0; i < 2; i++) {
        void *si = FindShopItem(ids[i]);
        if (si) return si;
    }
    return nullptr;
}

static NSString *ItemName(void *item) {
    if (!item || N.shop_name < 0) return nil;
    if (!sNames) sNames = [NSMutableDictionary dictionary];
    NSNumber *key = @(N.item_itemId >= 0 ? At<int>(item, N.item_itemId) : 0);
    NSString *cached = sNames[key];
    if (cached) return cached;
    void *si = ShopItemOf(item);
    NSString *name = si ? Str(At<void *>(si, N.shop_name)) : nil;
    if (name.length) sNames[key] = name;
    return name;
}

static int SkinDlcOf(void *shopItem) {       // the skin file the game would load, -1 = none
    if (!shopItem || !N.SI_getDLC) return -1;
    int type = DLC_TEXTURE, sub = 0;
    void *a[] = { &type, &sub };
    return InvokeInt(N.SI_getDLC, shopItem, a, -1);
}

static int TexcOff(void *shopBall) {         // mdl_Shop_Item_Ball.texc: which half of the file
    return shopBall ? FieldOffset(ClassOf(shopBall), "texc") : -1;
}

static int TexcOf(void *shopBall) {
    int o = TexcOff(shopBall);
    return o >= 0 ? (At<bool>(shopBall, o) ? 1 : 0) : -1;
}

static void *ShopItemsList() {               // ItemHolder._instance._shop._shopItems
    void *holder = ReadStaticObj(N.ih_instance, N.ItemHolder, "_instance");
    if (!holder || N.ih_shop < 0) return nullptr;
    void *shop = At<void *>(holder, N.ih_shop);
    if (!shop) return nullptr;
    static int itemsOff = -2;
    if (itemsOff == -2) itemsOff = FieldOffset(ClassOf(shop), "_shopItems");
    return itemsOff >= 0 ? At<void *>(shop, itemsOff) : nullptr;
}

// The skin catalog the game itself uses: ItemsVisualData, a ScriptableObject shipped in
// sharedassets0.assets (getDLC asks it through Director.DataObject). v1.1.1 asked
// DLCManager.all_server_dlc instead, which turned out to be empty on device.
static Ref sVisualData;
static void *VisualData() {
    void *v = sVisualData.get();
    if (v) return v;
    static int tries = 0;
    if (!N.IVD || !N.tIVD || N.ivd_items < 0 || tries > 20) return nullptr;
    tries++;
    Il2CppArray *all = FindAllLoaded(N.tIVD);
    int best = 0;
    for (size_t i = 0; i < Len(all); i++) {     // the loaded copy with the most entries
        void *o = Elem(all, i);
        ListView lv;
        if (Alive(o) && ReadList(At<void *>(o, N.ivd_items), lv) && lv.size > best) { best = lv.size; v = o; }
    }
    if (v) sVisualData.set(v);
    return v;
}

static NSString *DlcPath(int id) {           // the skin file's path inside the game's asset bundles
    if (id < 0 || !N.IVD_GetItemDataByID) return nil;
    void *v = VisualData();
    if (!v) return nil;
    void *a[] = { &id };
    void *rec = Invoke(N.IVD_GetItemDataByID, v, a);   // Models.mdl_DLS_Item
    if (!rec) return nil;
    static int urlOff = -2;
    if (urlOff == -2) urlOff = FieldOffset(ClassOf(rec), "dlc_url");
    return urlOff >= 0 ? Str(At<void *>(rec, urlOff)) : nil;
}

static NSSet<NSString *> *AppAssetNames() {   // file names of every asset shipped in the app's bundles
    static NSSet<NSString *> *names;
    if (names) return names;
    NSMutableSet<NSString *> *set = [NSMutableSet set];
    NSString *root = [[NSBundle mainBundle].bundlePath stringByAppendingPathComponent:@"Data/Raw/AssetBundles"];
    for (NSString *rel in [[NSFileManager defaultManager] enumeratorAtPath:root]) {
        if (![rel hasSuffix:@".manifest"]) continue;
        NSString *txt = [NSString stringWithContentsOfFile:[root stringByAppendingPathComponent:rel] encoding:NSUTF8StringEncoding error:nil];
        for (NSString *line in [txt componentsSeparatedByString:@"\n"]) {
            if ([line rangeOfString:@"- Assets/"].location == NSNotFound) continue;
            [set addObject:[[[line lastPathComponent] stringByDeletingPathExtension] lowercaseString]];
        }
    }
    names = set;
    return names;
}

static int Shipped(int id) {                 // 1 = its skin file ships with the app, 0 = it doesn't, -1 = can't tell
    NSString *p = DlcPath(id);
    NSSet<NSString *> *names = AppAssetNames();
    if (!p.length || !names.count) return -1;
    return [names containsObject:[[[p lastPathComponent] stringByDeletingPathExtension] lowercaseString]] ? 1 : 0;
}

// Two-ball files are named after both balls, first ball = left half = texc 0 (checked on device:
// Pearl texc 0 / Hybrid texc 1 on text_matchuppearl_matchuphybrid). Returns -1 if `key` isn't in the name.
static int HalfOf(NSString *path, NSString *key) {
    NSString *stem = [[[path lastPathComponent] stringByDeletingPathExtension] lowercaseString];
    NSMutableArray<NSString *> *parts = [[stem componentsSeparatedByString:@"_"] mutableCopy];
    while (parts.count && ([parts[0] isEqualToString:@"tex"] || [parts[0] isEqualToString:@"text"])) [parts removeObjectAtIndex:0];
    for (NSUInteger i = 0; i < parts.count; i++)
        if ([Squash(parts[i]) containsString:key]) return i == 0 ? 0 : 1;
    return -1;
}

static bool PathHas(NSString *path, NSString *key) { return path.length && [Squash(path) containsString:key]; }

// Make getDLC(TEXTURE) on this store entry return `want`, showing half `texc`.
static bool SetLink(void *item, int want, int texc, Link &L) {
    int to = TexcOff(item);
    void *links = (item && N.si_dlcLink >= 0) ? At<void *>(item, N.si_dlcLink) : nullptr;
    if (to < 0 || !links) return false;
    Il2CppClass *lk = ClassOf(links);
    const MethodInfo *add = FindMethod(lk, "Add", 1), *rem = FindMethod(lk, "Remove", 1);
    if (!add || !rem) return false;
    if (!L.changed) { L.changed = true; L.origDlc = SkinDlcOf(item); L.origTexc = TexcOf(item); }
    for (int guard = 0; guard < 4; guard++) {   // take off skins the game would pick before ours
        int cur = SkinDlcOf(item);
        if (cur < 0 || cur == want) break;
        int id = cur;
        void *a[] = { &id };
        Invoke(rem, links, a);
        if (L.nRemoved < 4) L.removed[L.nRemoved++] = cur;
    }
    if (SkinDlcOf(item) != want) {
        int id = want;
        void *a[] = { &id };
        Invoke(add, links, a);
        L.added = want;
    }
    At<bool>(item, to) = texc != 0;
    return SkinDlcOf(item) == want;
}

static void UndoLink(void *item, Link &L) {
    if (!L.changed) return;
    void *links = (item && N.si_dlcLink >= 0) ? At<void *>(item, N.si_dlcLink) : nullptr;
    if (links) {
        Il2CppClass *lk = ClassOf(links);
        const MethodInfo *add = FindMethod(lk, "Add", 1), *rem = FindMethod(lk, "Remove", 1);
        if (rem && L.added >= 0) { int id = L.added; void *a[] = { &id }; Invoke(rem, links, a); }
        for (int i = 0; add && i < L.nRemoved; i++) { int id = L.removed[i]; void *a[] = { &id }; Invoke(add, links, a); }
    }
    int to = TexcOff(item);
    if (item && to >= 0 && L.origTexc >= 0) At<bool>(item, to) = L.origTexc != 0;
    L = Link();
}

static void SkinDataTick() {
    if (!gBF.textureFix) {
        if (sPearlFail != PF_OFF) {
            UndoLink(sPearlShop.get(), sPearlLink);
            UndoLink(sBPShop.get(), sBPLink);
            sPearlFixed = sBPFixed = false;
            sPearlFail = PF_OFF;
            sScannedList = nullptr;   // check the store data again when it's turned back on
            sScannedCount = -1;
            sNeedRackRefresh = sNeedHandReload = true;
            BFLog(@"skin fix: off, the game's own links are back");
        }
        return;
    }
    if (sPearlFail == PF_OFF) sPearlFail = PF_WAITING;
    if (!sSettled || sFrame % 60 != 0) return;
    void *list = ShopItemsList();
    ListView lv;
    if (!list || !ReadList(list, lv) || lv.size == 0) return;
    if (list == sScannedList && lv.size == sScannedCount) return;   // nothing new since last time
    sScannedList = list;
    sScannedCount = lv.size;

    void *pearl = nullptr, *hybrid = nullptr, *bp = nullptr, *solid = nullptr;
    for (int i = 0; i < lv.size; i++) {
        void *it = lv.items[i];
        if (!it || N.shop_name < 0) continue;
        NSString *n = Squash(Str(At<void *>(it, N.shop_name)) ?: @"");
        if (![n hasPrefix:@"matchup"]) continue;
        if ([n isEqualToString:@"matchuppearl"]) pearl = it;
        else if ([n isEqualToString:@"matchuphybrid"]) hybrid = it;
        else if ([n isEqualToString:@"matchupsolid"]) solid = it;
        else if ([n containsString:@"black"] || [n hasSuffix:@"bp"]) bp = it;
    }
    if (pearl != sPearlShop.get()) sPearlLink = Link();   // new objects: nothing of ours on them
    if (bp != sBPShop.get()) sBPLink = Link();
    sPearlShop.set(pearl);
    sHybridShop.set(hybrid);
    sBPShop.set(bp);
    sSolidShop.set(solid);

    // what the game itself had (before any change of ours)
    int pd = sPearlLink.changed ? sPearlLink.origDlc : SkinDlcOf(pearl);
    int pt = sPearlLink.changed ? sPearlLink.origTexc : TexcOf(pearl);
    int bd = sBPLink.changed ? sBPLink.origDlc : SkinDlcOf(bp);
    int hd = SkinDlcOf(hybrid), ht = TexcOf(hybrid);
    sGamePearlDlc = pd;
    sGamePearlTexc = pt;

    // 1) Pearl -> the Pearl/Hybrid file (path checked when the catalog has it), left half
    sPearlFixed = false;
    sPearlSheet = -1;
    if (!pearl) {
        sPearlFail = PF_NO_PEARL;
    } else {
        NSString *pPath = DlcPath(pd), *hPath = DlcPath(hd);
        if (pd >= 0 && PathHas(pPath, kPearlSheetKey)) sPearlSheet = pd;            // right file, wrong half?
        else if (hd >= 0 && (!hPath.length || PathHas(hPath, kPearlSheetKey))) sPearlSheet = hd;
        if (sPearlSheet < 0) {
            sPearlFail = PF_NO_SHEET;
            BFLog(@"skin fix: no Pearl/Hybrid file found (pearl %d %@, hybrid %d %@)", pd, pPath, hd, hPath);
        } else {
            // the Pearl is the left half; the Hybrid (right half) is the reference when it shares the file
            int want = (sPearlSheet == hd && ht >= 0) ? (ht ? 0 : 1) : 0;
            if (SkinDlcOf(pearl) == sPearlSheet && TexcOf(pearl) == want) sPearlFixed = true;
            else {
                sPearlFixed = SetLink(pearl, sPearlSheet, want, sPearlLink);
                sNeedRackRefresh = sNeedHandReload = true;
            }
            sPearlFail = sPearlFixed ? PF_OK : PF_LINK_FAILED;
            BFLog(@"skin fix: Pearl -> skin %d half %d (game had %d/%d %@) ok=%d", sPearlSheet, want, pd, pt, pPath ?: @"-", sPearlFixed);
        }
    }

    // 2) BP: its own skin file isn't in the app (1.907: id 413 = black_pearl2_dif_x512.jpg), so it never
    //    loads: white on the rack, or the last ball's skin in your hand. The real Black Pearl art ships as
    //    the left half of tex_blackpearl_flameturquoise.png (id 770), the file the game had put on the Pearl.
    sBPFixed = false;
    int bs = bd >= 0 ? Shipped(bd) : 0;           // 1 fine, 0 broken, -1 can't tell
    if (bp && bs != 1) {
        int target = -1, half = 0;
        int cands[2] = { pd, kKnownBlackPearlSkin };
        for (int c : cands) {
            if (c < 0 || c == sPearlSheet) continue;
            NSString *path = DlcPath(c);
            if (!PathHas(path, @"blackpearl") || Shipped(c) == 0) continue;
            int h = HalfOf(path, @"blackpearl");
            target = c;
            half = h >= 0 ? h : 0;
            break;
        }
        if (target < 0 && bs == -1 && pd >= 0 && pd != sPearlSheet) { target = pd; half = pt == 1 ? 1 : 0; }   // catalog unreadable
        if (target >= 0) {
            sBPFixed = SetLink(bp, target, half, sBPLink);
            sNeedRackRefresh = sNeedHandReload = true;
            BFLog(@"skin fix: BP skin %d (shipped=%d) -> %d half %d ok=%d", bd, bs, target, half, sBPFixed);
        }
    }
    sBPHasSkin = bs == 1 || sBPFixed;
}

static void CurrentBallTick() {
    if (!sSettled || sFrame % 20 != 0) return;
    // the game's own cached "current ball" (its getter can make the game load things early)
    void *item = ReadStaticObj(N.inv_currentBall, N.InvHelper, "_currentBall");
    if (item == sLastItem) return;
    NSString *nm = ItemName(item);
    sLastItem = nm ? item : nullptr;        // retry later if the store data isn't ready yet
    sCurSkin = SkinForName(nm);
    sBallLine = nm.length ? [NSString stringWithFormat:@"Current ball: %@", nm] : @"";
}

static void ReloadInHand(void *shopItem) {   // same call the game makes when you pick up a ball
    void *rpt = sRPT.get();
    if (!rpt || !shopItem || !N.LDT_loadForItem) return;
    int sw = TexcOf(shopItem) == 1 ? 1 : 0;
    int offs[2] = { N.rpt_sphereRender, N.rpt_sphere };
    void *done = nullptr;
    for (int i = 0; i < 2; i++) {
        if (offs[i] < 0) continue;
        void *comp = GetCompInChildren(At<void *>(rpt, offs[i]), N.tLoadDLCTex);
        if (!comp || comp == done) continue;
        done = comp;
        int sub = 0;
        void *a[] = { shopItem, &sub, &sw };
        Invoke(N.LDT_loadForItem, comp, a);
    }
}

static void SkinRefreshTick() {
    if (!sSettled || sFrame % 15 != 0) return;
    if (sNeedHandReload) {
        sNeedHandReload = false;
        if (sCurSkin == SKIN_PEARL) ReloadInHand(sPearlShop.get());
        else if (sCurSkin == SKIN_BP) ReloadInHand(sBPShop.get());
    }
    if (!sNeedRackRefresh) return;
    if (sLoc != LOC_UPPER_SCREEN && sLoc != LOC_BALL_RETURNER) return;   // only while you're at the rack / top screen
    sNeedRackRefresh = false;
    void *inv = ReadStaticObj(N.invd_instance, N.InventaryData, "_instance");
    if (!inv || !N.InvD_InitBallsOnReturner) return;
    bool force = true;
    void *a[] = { &force };
    Invoke(N.InvD_InitBallsOnReturner, inv, a);   // same call the game makes when your inventory changes
    BFLog(@"skin fix: refreshed the ball rack");
}

// ---- last resort, ball in your hand only: Pearl if its file can't be linked, BP if it has no skin ----
static void *LoadPearlFrom(void *bundle) {
    if (!Alive(bundle) || !N.AB_LoadAsset || !N.tTexture2D) return nullptr;
    void *a[] = { NewString(kPearlAsset), N.tTexture2D };
    void *t = Invoke(N.AB_LoadAsset, bundle, a);
    return Alive(t) ? t : nullptr;
}

static void *LoadPearlTexture() {
    Il2CppArray *bundles = FindAllLoaded(N.tAssetBundle);      // 1) bundle already open?
    for (size_t i = 0; i < Len(bundles); i++) {
        void *b = Elem(bundles, i);
        NSString *nm = NameOf(b);
        if (nm && [nm rangeOfString:@"fulltextures"].location != NSNotFound) {
            void *t = LoadPearlFrom(b);
            if (t) return t;
        }
    }
    if (N.AB_LoadFromFile && N.AB_Unload) {                    // 2) open it, copy the texture, close it
        NSString *path = [[NSBundle mainBundle].bundlePath stringByAppendingPathComponent:@"Data/Raw/AssetBundles/balls/fulltextures"];
        void *a[] = { NewString(path.UTF8String) };
        void *b = Invoke(N.AB_LoadFromFile, nullptr, a);
        if (Alive(b)) {
            void *t = LoadPearlFrom(b);
            bool unloadEverything = false;
            void *u[] = { &unloadEverything };
            Invoke(N.AB_Unload, b, u);
            if (t) return t;
        }
    }
    Il2CppArray *texs = FindAllLoaded(N.tTexture2D);            // 3) already in memory?
    for (size_t i = 0; i < Len(texs); i++) {
        void *t = Elem(texs, i);
        if ([NameOf(t) isEqualToString:kPearlTexName]) return t;
    }
    return nullptr;
}

static void *PearlTexture() {
    void *t = sPearlTex ? Target(sPearlTex) : nullptr;
    if (Alive(t)) return t;
    if (sPearlTex) { Release(sPearlTex); sPearlTex = 0; }
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    if (sPearlTries >= 5 || now < sPearlNextTry) return nullptr;
    sPearlTries++;
    sPearlNextTry = now + 4;
    t = LoadPearlTexture();
    if (t) { DontUnload(t); sPearlTex = Keep(t); BFLog(@"Match Up Pearl sheet loaded (last resort)"); }
    return t;
}

static void *BPTexture() {
    void *t = sBPTex ? Target(sBPTex) : nullptr;
    if (Alive(t)) return t;
    if (sBPTex) { Release(sBPTex); sBPTex = 0; }
    if (sBPTries >= 3 || !N.Texture2D || !N.T2D_ctor || !N.LoadImage || !N.Byte) return nullptr;
    sBPTries++;
    NSData *png = BFMakeMatchUpBPTexturePNG(512);
    if (!png.length) return nullptr;
    Il2CppObject *tex = NewObject(N.Texture2D);
    if (!tex) return nullptr;
    int w = 2, h = 2;
    bool ok = false;
    void *ca[] = { &w, &h };
    Invoke(N.T2D_ctor, tex, ca, &ok);
    if (!ok) return nullptr;
    Il2CppArray *bytes = NewArray(N.Byte, png.length);
    if (!bytes) return nullptr;
    memcpy(Data(bytes), png.bytes, png.length);
    void *la[] = { tex, bytes };
    if (!InvokeBool(N.LoadImage, nullptr, la, false)) return nullptr;
    DontUnload(tex);
    sBPTex = Keep(tex);
    BFLog(@"Match Up BP look-alike skin created (last resort)");
    return tex;
}

static void *MaterialOf(void *go) {
    void *r = GetComp(go, N.tRenderer);
    return (r && N.R_getMaterial) ? (void *)Invoke(N.R_getMaterial, r, nullptr) : nullptr;
}

// Hands a texture to the ball's own LoadDLCContentTexture.textureLoaded(), the code the game
// runs when a skin finishes loading. switchId (0/1, -1 = keep) is the same "which half" flag.
static void ApplySkinIfNeeded(void *comp, void *tex, int switchId) {
    if (!Alive(comp) || !tex || N.ldt_toChange < 0 || !N.LDT_textureLoaded) return;
    Il2CppArray *targets = At<Il2CppArray *>(comp, N.ldt_toChange);
    if (!Len(targets)) return;
    void *mat0 = MaterialOf(Elem(targets, 0));
    if (!mat0) return;
    if (N.M_getMainTex && (void *)Invoke(N.M_getMainTex, mat0, nullptr) == tex) return;   // already showing it
    if (switchId >= 0 && N.ldt_objectID >= 0) At<int>(comp, N.ldt_objectID) = switchId;
    void *a[] = { tex };
    Invoke(N.LDT_textureLoaded, comp, a);
}

static Ref sHandComp[2];

static void InHandFallbackTick() {           // every frame, so a wrong skin never flashes
    if (!gBF.textureFix || !sSettled || sPearlFail == PF_WAITING) return;
    bool pearl = sCurSkin == SKIN_PEARL && !sPearlFixed && (sPearlFail == PF_NO_SHEET || sPearlFail == PF_LINK_FAILED);
    bool bp = sCurSkin == SKIN_BP && !sBPHasSkin;
    if (!pearl && !bp) return;
    void *tex = bp ? BPTexture() : PearlTexture();
    if (!tex) return;
    if (sFrame % 30 == 0 || !sHandComp[0].get()) {
        void *rpt = sRPT.get();
        if (!rpt) return;
        int offs[2] = { N.rpt_sphereRender, N.rpt_sphere };
        for (int i = 0; i < 2; i++)
            sHandComp[i].set(offs[i] >= 0 ? GetCompInChildren(At<void *>(rpt, offs[i]), N.tLoadDLCTex) : nullptr);
    }
    int sw = bp ? -1 : 0;   // Pearl = left half (texc 0); the BP look-alike has the same art on both halves
    void *first = sHandComp[0].get();
    if (first) ApplySkinIfNeeded(first, tex, sw);
    void *second = sHandComp[1].get();
    if (second && second != first) ApplySkinIfNeeded(second, tex, sw);
}

// ---------------------------------------------------------------------------
// Skip tutorial
// A fresh install makes you play the tutorial before you can log in. The game already has a
// complete skip (TutorialManager.SkipTutorial: marks it done, restarts the lane, goes to the
// top screen in normal play). We just show a button for it.
// Careful: IsInTutorial() only checks the tutorial's "active stage", and SkipTutorial() never
// clears that, so it keeps saying yes after a skip (v1.1.0's button got stuck on screen).
// The game's own "tutorial completed" flag is the reliable part.
// ---------------------------------------------------------------------------
static Ref sTutorial;
static bool sInTutorial = false, sSkipRequested = false, sSkipDone = false;

void BFRequestSkipTutorial(void) { sSkipRequested = true; }

// SkipTutorial() leaves its active stage behind. That stage then "finishes" on your first
// throw, and the game's normal end-of-stage cleanup restarts the lane, kicking you back to the
// main menu once. We do that cleanup right away, like HandleTutorialStageEnded does:
// unhook the stage's OnStageEnded and clear _activeStage (every handler null-checks it).
static void ClearTutorialStage(void *tm) {
    if (!tm || N.tm_activeStage < 0) return;
    void *stage = At<void *>(tm, N.tm_activeStage);
    if (!stage) return;
    int ended = FieldOffset(ClassOf(stage), "OnStageEnded");
    if (ended >= 0) At<void *>(stage, ended) = nullptr;   // storing null needs no GC write barrier
    At<void *>(tm, N.tm_activeStage) = nullptr;
    BFLog(@"tutorial: cleared the stage SkipTutorial left behind");
}

static void TutorialTick() {
    if (!N.Tutorial || !N.TM_IsInTutorial) return;
    if (!sSkipRequested && sFrame % 30 != 0) return;
    void *tm = sTutorial.get();
    if (!tm && !sSkipDone && sFrame % 60 == 0) { tm = FirstAlive(FindAll(N.tTutorial)); sTutorial.set(tm); }
    bool in = sSettled && tm && !sSkipDone && InvokeBool(N.TM_IsInTutorial, tm, nullptr, false);
    if (in && N.CSM_TutorialDone && InvokeBool(N.CSM_TutorialDone, nullptr, nullptr, false)) in = false;
    if (sSkipRequested) {
        sSkipRequested = false;
        if (in && N.TM_SkipTutorial) {
            BFLog(@"skipping the tutorial (the game's own SkipTutorial)");
            Invoke(N.TM_SkipTutorial, tm, nullptr);
            ClearTutorialStage(tm);
        }
        sSkipDone = true;     // never show the button again this session
        in = false;
    }
    if (in != sInTutorial) {
        sInTutorial = in;
        BFMenuSetSkipTutorialVisible(in);
    }
}

// ---------------------------------------------------------------------------
// Don't get stuck connecting
// With some networks (seen with NextDNS DNS-over-HTTPS) the connection neither works nor cleanly
// fails. The loading screen only offers its gray "play offline" button once the connector gives up
// for good (LoadingWindow.HandleServerDisconnected checks Connector.WithoutReconnect); otherwise it
// keeps reconnecting forever. The startup wait for the privacy SDK (InitState.WaitGDRPCallback)
// has no time limit either. With no internet at all both fail fast, which is why that case works.
// So we add the time limits the game is missing:
//  - loading screen on "connecting" for 30 s straight -> show the game's own offline section
//    (SwitchToSection(_offlineSection)). If the connection comes through later, the game
//    switches sections itself.
//  - privacy SDK silent for 20 s while no privacy page is on screen -> mark the wait done, which
//    is what happens when the SDK can't reach its server.
// ---------------------------------------------------------------------------
static Ref sCoreLoop, sLoadingWnd;
static CFAbsoluteTime sGdprWaitSince = 0, sConnectingSince = 0;
static int sUnstuckCount = 0;

static bool GOActive(void *go) {
    return go && N.GO_activeInHierarchy && InvokeBool(N.GO_activeInHierarchy, go, nullptr, false);
}

static bool SectionActive(void *lw, int off) { return off >= 0 && GOActive(At<void *>(lw, off)); }

static void StuckTick() {
    if (!gBF.unstick || sFrame % 30 != 0) return;
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();

    // 1) startup wait for the privacy SDK
    if (!sSettled && N.MonoCoreLoop && N.InitState && N.mcl_sm >= 0 && N.sm_state >= 0 && N.is_gdpr >= 0) {
        void *mcl = sCoreLoop.get();
        if (!mcl && sFrame % 60 == 0) { mcl = FirstAlive(FindAll(N.tMonoCoreLoop)); sCoreLoop.set(mcl); }
        void *sm = mcl ? At<void *>(mcl, N.mcl_sm) : nullptr;
        void *state = sm ? At<void *>(sm, N.sm_state) : nullptr;
        bool waiting = state && ClassOf(state) == N.InitState && !At<bool>(state, N.is_gdpr);
        if (!waiting || BFPrivacyPageVisible()) sGdprWaitSince = 0;   // never cut short a page you're reading
        else if (!sGdprWaitSince) sGdprWaitSince = now;
        else if (now - sGdprWaitSince > 20) {
            At<bool>(state, N.is_gdpr) = true;
            sGdprWaitSince = 0;
            sUnstuckCount++;
            BFLog(@"loading: privacy SDK silent for 20 s, carrying on (like with no internet)");
        }
    }

    // 2) loading screen stuck on "connecting"
    if (!N.LoadingWindow || !N.LW_Switch || N.lw_offline < 0) return;
    void *lw = sLoadingWnd.get();
    if (!lw && sFrame % 60 == 0) { lw = FirstAlive(FindAll(N.tLoadingWindow)); sLoadingWnd.set(lw); }
    bool stuck = lw && (SectionActive(lw, N.lw_conn) || SectionActive(lw, N.lw_noConn)) &&
                 !SectionActive(lw, N.lw_offline) && !SectionActive(lw, N.lw_login) && !SectionActive(lw, N.lw_online);
    if (!stuck) { sConnectingSince = 0; return; }
    if (!sConnectingSince) { sConnectingSince = now; return; }
    if (now - sConnectingSince < 30) return;
    void *a[] = { At<void *>(lw, N.lw_offline) };
    Invoke(N.LW_Switch, lw, a);
    sConnectingSince = 0;   // if it goes back to "connecting", wait another 30 s
    sUnstuckCount++;
    BFLog(@"loading: stuck connecting for 30 s, showing the game's offline button");
}

// ---------------------------------------------------------------------------
// Spinner watchdog
// The white loading circle (Processing) shows while the game waits for a server answer
// (e.g. PracticeOil.HandleApplyClicked -> Processing.Show -> PracticeManager.PlayPractice ->
// Connector.Sender). It has no time limit, so a request lost on a dead connection spins forever.
// After 30 s we hide it (Processing.Hide until its count is 0; Hide floors the count at 0, so a
// late answer can't break the next spinner). You're back on the screen you came from and can retry.
// ---------------------------------------------------------------------------
static FieldInfo *sProcInst;
static CFAbsoluteTime sSpinnerSince = 0;
static int sSpinnerCount = 0;

static void SpinnerTick() {
    if (sFrame % 30 != 0 || !sSettled || !N.Processing || !N.Proc_Hide || N.proc_count < 0) return;
    void *p = ReadStaticObj(sProcInst, N.Processing, "_instance");
    sSpinnerCount = (p && Alive(p)) ? At<int>(p, N.proc_count) : 0;
    if (!gBF.unstick || sSpinnerCount <= 0) { sSpinnerSince = 0; return; }
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    if (!sSpinnerSince) { sSpinnerSince = now; return; }
    if (now - sSpinnerSince < 30) return;
    int was = sSpinnerCount;
    for (int i = 0; i < 10 && At<int>(p, N.proc_count) > 0; i++) Invoke(N.Proc_Hide, p, nullptr);
    sSpinnerSince = 0;
    sUnstuckCount++;
    BFLog(@"spinner: no server answer for 30 s, hid it so you can try again (count was %d)", was);
}

// ---------------------------------------------------------------------------
// Diagnostics: write the loading/connection state to the log whenever it changes
// (core loop state, loading screen sections, Connector + ReconnectManager statics, the
// Photon peer's server and protocol, iOS reachability, which game windows are open).
// Connector/ReconnectManager are only read once the game itself is using them (loading
// window up, or startup past InitState): reading a class's statics runs its static setup.
// The game's own Logger is turned up to "All" for the first 2 minutes so its messages land
// in the captured console output.
// ---------------------------------------------------------------------------
static CFAbsoluteTime sStartTime = 0;   // first engine tick (about when the game starts)
static NSString *sLastDiag, *sLastWindows;
static FieldInfo *sCsState, *sCsConnecting, *sCsDoReconnect, *sCsDiscByServer, *sCsWithout, *sCsRedirect, *sCsPeer, *sCsRelays, *sCsClientApi, *sCsServerApi;
static FieldInfo *sRmState, *sRmCount, *sRmEnd, *sLgLevel, *sLgGlobal;
static int sLoggerOrig = -100;
static bool sLoggerRestored = false;

static int SInt(Il2CppClass *k, FieldInfo *&f, const char *name) {
    if (!k) return -99;
    if (!f) f = StaticField(k, name);
    int v = -99;
    if (f) StaticRead(f, &v);
    return v;
}
static int SBool(Il2CppClass *k, FieldInfo *&f, const char *name) {
    if (!k) return -1;
    if (!f) f = StaticField(k, name);
    uint8_t v = 0;
    if (!f) return -1;
    StaticRead(f, &v);
    return v ? 1 : 0;
}
static NSString *SStr(Il2CppClass *k, FieldInfo *&f, const char *name) {
    void *o = ReadStaticObj(f, k, name);
    return o ? (Str(o) ?: @"?") : @"-";
}

static NSString *QueueStrings(void *q) {   // System.Collections.Generic.Queue<string>, read by field names
    if (!q) return @"-";
    Il2CppClass *k = ClassOf(q);
    int arrOff = FieldOffset(k, "_array"), headOff = FieldOffset(k, "_head"), sizeOff = FieldOffset(k, "_size");
    if (arrOff < 0 || headOff < 0 || sizeOff < 0) return @"?";
    Il2CppArray *arr = At<Il2CppArray *>(q, arrOff);
    int head = At<int>(q, headOff), size = At<int>(q, sizeOff);
    size_t len = Len(arr);
    if (!arr || size < 0 || len == 0 || (size_t)size > len || head < 0 || (size_t)head >= len) return @"?";
    NSMutableArray *out = [NSMutableArray array];
    for (int i = 0; i < size && i < 8; i++) [out addObject:Str(((void **)Data(arr))[((size_t)head + i) % len]) ?: @"?"];
    return [NSString stringWithFormat:@"[%@]", [out componentsJoinedByString:@" "]];
}

static void GameLoggerTick() {   // more game output for the first 2 minutes
    if (!N.GameLogger || sLoggerRestored) return;
    if (!sLgLevel) sLgLevel = StaticField(N.GameLogger, "CommonMaxLoggingLevel");
    if (!sLgLevel) return;
    static int levelOff = -2;
    void *global = ReadStaticObj(sLgGlobal, N.GameLogger, "Global");
    if (levelOff == -2) levelOff = FieldOffset(N.GameLogger, "_loggingLevel");
    if (sLoggerOrig == -100) {
        StaticRead(sLgLevel, &sLoggerOrig);
        BFLog(@"game logger: level was %d, set to 0 (All) for 2 minutes", sLoggerOrig);
    }
    int all = 0;
    if (CFAbsoluteTimeGetCurrent() - sStartTime < 120) {
        StaticWrite(sLgLevel, &all);
        if (global && levelOff >= 0) At<int>(global, levelOff) = 0;
    } else {
        StaticWrite(sLgLevel, &sLoggerOrig);
        sLoggerRestored = true;
        BFLog(@"game logger: back to level %d", sLoggerOrig);
    }
}

static void DiagTick() {
    if (sFrame % 30 != 0) return;
    GameLoggerTick();
    // where startup is
    NSString *core = @"-";
    bool pastInit = sSettled;
    void *mcl = sCoreLoop.get();
    if (!mcl && N.tMonoCoreLoop && sFrame % 60 == 0) { mcl = FirstAlive(FindAll(N.tMonoCoreLoop)); sCoreLoop.set(mcl); }
    void *sm = (mcl && N.mcl_sm >= 0) ? At<void *>(mcl, N.mcl_sm) : nullptr;
    void *state = (sm && N.sm_state >= 0) ? At<void *>(sm, N.sm_state) : nullptr;
    if (state) {
        core = [NSString stringWithUTF8String:ClassName(ClassOf(state))];
        if (ClassOf(state) == N.InitState && N.is_gdpr >= 0) core = [core stringByAppendingFormat:@"(gdprDone=%d)", At<bool>(state, N.is_gdpr) ? 1 : 0];
        else pastInit = true;
    }
    void *lw = sLoadingWnd.get();
    if (!lw && N.tLoadingWindow && sFrame % 60 == 0) { lw = FirstAlive(FindAll(N.tLoadingWindow)); sLoadingWnd.set(lw); }
    NSString *load = @"-";
    if (lw) {
        pastInit = true;
        load = [NSString stringWithFormat:@"%c%c%c%c%c",
                SectionActive(lw, N.lw_noConn) ? 'N' : 'n', SectionActive(lw, N.lw_conn) ? 'C' : 'c', SectionActive(lw, N.lw_login) ? 'L' : 'l',
                SectionActive(lw, N.lw_online) ? 'O' : 'o', SectionActive(lw, N.lw_offline) ? 'F' : 'f'];
    }
    NSString *conn = @"(not started)";
    if (pastInit && N.Connector) {
        static const char *names[] = { "OFFLINE", "MASTER_CONNECTING", "MASTER_CONNECTED", "GAME_CONNECTING", "GAME_CONNECTED" };
        int st = SInt(N.Connector, sCsState, "_state");
        void *peer = ReadStaticObj(sCsPeer, N.Connector, "peer");
        NSString *server = @"-";
        int proto = -1, v6 = -1;
        if (peer) {
            static int pbOff = -2;
            static const MethodInfo *isV6 = nullptr;
            if (pbOff == -2) pbOff = FieldOffset(ClassOf(peer), "peerBase");
            void *pb = pbOff >= 0 ? At<void *>(peer, pbOff) : nullptr;
            if (pb && !isV6) isV6 = FindMethod(ClassOf(pb), "get_IsIpv6", 0);
            if (pb && isV6) v6 = InvokeBool(isV6, pb, nullptr, false) ? 1 : 0;
            static const MethodInfo *addr = nullptr, *tp = nullptr;
            if (!addr) addr = FindMethod(ClassOf(peer), "get_ServerAddress", 0);
            if (!tp) tp = FindMethod(ClassOf(peer), "get_TransportProtocol", 0);
            if (addr) server = Str(Invoke(addr, peer, nullptr)) ?: @"?";
            if (tp) proto = InvokeInt(tp, peer, nullptr, -1);
        }
        NSString *relays = QueueStrings(ReadStaticObj(sCsRelays, N.Connector, "_relays"));
        conn = [NSString stringWithFormat:@"%s(%d) connecting=%d reconnect=%d discByServer=%d noReconnect=%d server=%@ proto=%d ipv6=%d redirect=%@ relays=%@ api=%@/%@",
                st >= 0 && st <= 4 ? names[st] : "?", st, SBool(N.Connector, sCsConnecting, "_isConnecting"),
                SBool(N.Connector, sCsDoReconnect, "doReconnect"), SBool(N.Connector, sCsDiscByServer, "wasDiscByServer"),
                SBool(N.Connector, sCsWithout, "WithoutReconnect"), server, proto, v6, SStr(N.Connector, sCsRedirect, "redirectTo"), relays,
                SStr(N.Connector, sCsClientApi, "Client_API_Version"), SStr(N.Connector, sCsServerApi, "Server_API_Version")];
        if (N.Reconnect)
            conn = [conn stringByAppendingFormat:@" | reconnectMgr state=%d count=%d end=%d", SInt(N.Reconnect, sRmState, "reconnectState"),
                    SInt(N.Reconnect, sRmCount, "reconnectCount"), SInt(N.Reconnect, sRmEnd, "reconnectSequenceEnd")];
    }
    int reach = N.App_reach ? InvokeInt(N.App_reach, nullptr, nullptr, -1) : -1;   // 0 none, 1 cellular, 2 wifi
    NSString *diag = [NSString stringWithFormat:@"core=%@ | loading=%@ | conn=%@ | reach=%d | spinner=%d", core, load, conn, reach, sSpinnerCount];
    if (![diag isEqualToString:sLastDiag]) {
        sLastDiag = diag;
        BFLogEvent(@"game", diag);
    }
    // which game windows are open (every 2 s)
    if (sFrame % 120 == 0 && N.tFloatWnd) {
        NSMutableArray<NSString *> *names = [NSMutableArray array];
        Il2CppArray *all = FindAll(N.tFloatWnd);
        for (size_t i = 0; i < Len(all); i++) {
            void *w = Elem(all, i);
            if (Alive(w) && GOActive(GameObjectOf(w))) [names addObject:[NSString stringWithUTF8String:ClassName(ClassOf(w))]];
        }
        NSString *list = names.count ? [[names sortedArrayUsingSelector:@selector(compare:)] componentsJoinedByString:@", "] : @"(none)";
        if (![list isEqualToString:sLastWindows]) {
            sLastWindows = list;
            BFLogEvent(@"ui", [@"windows open: " stringByAppendingString:list]);
        }
    }
}

// ---------------------------------------------------------------------------
// 120 FPS mode
// The game takes its frame rate from a table in Client.Core.Constants: on phones MENU_FRAME_RATE
// = 30 and GAME_FRAME_RATE = 60 (the PC / Facebook GameRoom build used FB_FRAME_RATE = 200). We
// set the phone values to 120, so the game applies 120 itself at every screen change, and we
// re-apply it if anything sets a lower rate. iOS only allows more than 60 Hz when Info.plist has
// CADisableMinimumFrameDurationOnPhone = YES, which the patched IPA sets.
// ---------------------------------------------------------------------------
static FieldInfo *sGameFpsF, *sMenuFpsF;
static int sOrigGameFps = -1, sOrigMenuFps = -1;
static bool sFpsApplied = false;

static bool PlistAllows120() {
    return [[[NSBundle mainBundle] objectForInfoDictionaryKey:@"CADisableMinimumFrameDurationOnPhone"] boolValue];
}

static void SetTargetFps(int v) {
    void *a[] = { &v };
    Invoke(N.App_setFps, nullptr, a);
}

static void FpsTick() {
    if (!sSettled || sFrame % 30 != 0 || !N.Constants || !N.App_setFps) return;
    if (!sGameFpsF) sGameFpsF = StaticField(N.Constants, "GAME_FRAME_RATE");
    if (!sMenuFpsF) sMenuFpsF = StaticField(N.Constants, "MENU_FRAME_RATE");
    if (!sGameFpsF || !sMenuFpsF) return;
    if (gBF.fps120) {
        if (!sFpsApplied) {
            StaticRead(sGameFpsF, &sOrigGameFps);
            StaticRead(sMenuFpsF, &sOrigMenuFps);
            int v = 120;
            StaticWrite(sGameFpsF, &v);
            StaticWrite(sMenuFpsF, &v);
            sFpsApplied = true;
            BFLog(@"120 FPS: frame rate table menu %d / game %d -> 120", sOrigMenuFps, sOrigGameFps);
        }
        if (InvokeInt(N.App_getFps, nullptr, nullptr, 120) != 120) SetTargetFps(120);
    } else if (sFpsApplied) {
        int game = sOrigGameFps > 0 ? sOrigGameFps : 60, menu = sOrigMenuFps > 0 ? sOrigMenuFps : 30;
        StaticWrite(sGameFpsF, &game);
        StaticWrite(sMenuFpsF, &menu);
        SetTargetFps(game);   // the game sets its own value again at the next screen change
        sFpsApplied = false;
        BFLog(@"120 FPS: off, frame rate table back to %d / %d", menu, game);
    }
}

NSString *BFFpsLine(void) {
    long screenMax = (long)[UIScreen mainScreen].maximumFramesPerSecond;
    if (!PlistAllows120()) return @"This copy of the game isn't allowed above 60 Hz. Install the new patched IPA for 120 FPS.";
    if (screenMax < 120) return [NSString stringWithFormat:@"This screen tops out at %ld Hz.", screenMax];
    if (!gBF.fps120) return @"Off: the game's normal 30 FPS menus / 60 FPS gameplay.";
    if (!sFpsApplied) return @"On: applies once the lane has loaded.";
    int cur = N.App_getFps ? InvokeInt(N.App_getFps, nullptr, nullptr, -1) : -1;
    return [NSString stringWithFormat:@"On: game asks for %d FPS (screen max %ld Hz).", cur, screenMax];
}

// ---------------------------------------------------------------------------
// Arsenal search
// The Arsenal list draws from ArsenalBallManager._ballsData (a List of your balls).
// We keep only the matching balls in that list and tell the scroll view to reload.
// InitScrollData() puts the full list back.
// ---------------------------------------------------------------------------
static NSString *sQuery = nil, *sAppliedQuery = nil;
static Ref sArsenal;
static void *sFilteredList = nullptr;
static int sFilteredVersion = 0, sShown = -1, sTotal = -1;
static bool sArsenalDirty = false;

void BFSetArsenalQuery(NSString *q) {
    q = [q stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    sQuery = q.length ? [q copy] : nil;
    sArsenalDirty = true;
}

// The game itself calls ResetScroll() after building the list: it re-counts the items and
// resizes the scroll area. (v1.0 called ReloadData(), which only redraws the cells already on
// screen, so the list kept the old size: empty space and missing balls when scrolling.)
static void ResetArsenalScroll(void *ars) {
    if (N.ars_scroll < 0) return;
    void *scroll = At<void *>(ars, N.ars_scroll);
    if (!Alive(scroll)) return;
    Il2CppClass *k = ClassOf(scroll);
    const MethodInfo *m = FindMethod(k, "ResetScroll", 0);
    if (!m) m = FindMethod(k, "ReloadData", 0);
    if (m) Invoke(m, scroll, nullptr);
}

static void ArsenalTick() {
    if (!N.Arsenal || N.ars_ballsData < 0 || !sSettled) return;
    if (!sArsenalDirty && sFrame % 15 != 0) return;
    if (!sQuery && !sAppliedQuery) { sArsenalDirty = false; return; }
    void *ars = sArsenal.get();
    if (!ars && (sArsenalDirty || sFrame % 30 == 0)) {
        ars = FirstAlive(FindAll(N.tArsenal));
        sArsenal.set(ars);
    }
    sArsenalDirty = false;
    if (!ars) { sFilteredList = nullptr; sAppliedQuery = nil; sShown = sTotal = -1; return; }

    if (!sQuery) {                                    // search cleared: put the full list back
        if (N.Ars_InitScrollData) { Invoke(N.Ars_InitScrollData, ars, nullptr); ResetArsenalScroll(ars); }
        sAppliedQuery = nil; sFilteredList = nullptr; sShown = sTotal = -1;
        return;
    }
    void *list = At<void *>(ars, N.ars_ballsData);
    ListView lv;
    if (!ReadList(list, lv)) return;
    bool rebuilt = list != sFilteredList || lv.version != sFilteredVersion;   // the game refreshed the list
    if (!rebuilt && [sQuery isEqualToString:sAppliedQuery]) return;
    if (!rebuilt) {                                    // new search on an already-filtered list
        if (!N.Ars_InitScrollData) return;
        Invoke(N.Ars_InitScrollData, ars, nullptr);
        list = At<void *>(ars, N.ars_ballsData);
        if (!ReadList(list, lv)) return;
    }
    NSMutableArray<NSString *> *words = [NSMutableArray array];
    for (NSString *w in [sQuery componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]) {
        NSString *s = Squash(w);
        if (s.length) [words addObject:s];
    }
    int keep = 0;
    for (int i = 0; i < lv.size; i++) {
        void *it = lv.items[i];
        if (!it) continue;
        NSString *name = Squash(ItemName(it) ?: @"");
        bool match = true;
        for (NSString *w in words) if (![name containsString:w]) { match = false; break; }
        if (match) lv.items[keep++] = it;
    }
    sTotal = lv.size;
    sShown = keep;
    At<int>(list, lv.sizeOff) = keep;
    if (lv.versionOff >= 0) At<int>(list, lv.versionOff) += 1;
    ResetArsenalScroll(ars);
    sFilteredList = list;
    sFilteredVersion = lv.versionOff >= 0 ? At<int>(list, lv.versionOff) : 0;
    sAppliedQuery = sQuery;
    BFLog(@"arsenal search '%@': %d of %d balls", sQuery, sShown, sTotal);
}

// ---------------------------------------------------------------------------
// Oil (practice)
// How the game's oil works (see VERIFIED_NOTES.md section 12):
//  - Each pattern is an OilDescription (OilDescriptionData.Oils) with a live grid OilMatrix
//    [board, row] and a clean copy _sourceOilMatrix. GameParams.OIL_SELECTED is 1 + its index.
//  - While the ball touches the lane, BallSoundManager moves 20% of the oil from the cell it just
//    left to the cell it's on (breakdown + carrydown). A new game copies the clean grid back.
//  - The lane shader draws the grid with u = 1 - x/_SizeX, which mirrors it left/right against the
//    physics (board = (x + W/2)/W * columns). Symmetric patterns hide it; the ball track and
//    carrydown show up on the wrong side. Fix: _SizeX < 0 and the oil texture on Repeat, so
//    u = 1 + x/_SizeX wraps to exactly x/_SizeX (the mesh spans 0..SizeX).
//  - Patterns are real Kegel lane-machine files, turned into the grid at runtime by the game's
//    Kegel engine: Drawer.drawFromFile -> Pattern (forward/reverse PatternLoadScreens of
//    start board, stop board, loads, speed) -> Units; OilMatrix[w,h] = Units[w + 1, h / 4].
// Everything here changes the game only in offline practice and is undone when you leave it.
// ---------------------------------------------------------------------------
struct OilGrid { void *arr = nullptr; float *data = nullptr; int w = 0, h = 0; };

static bool GridOf(void *arr, OilGrid &g) {      // float[,]: bounds at +0x10, data at +0x20
    g = OilGrid();
    if (!arr) return false;
    uint8_t *bounds = At<uint8_t *>(arr, 0x10);
    if (!bounds) return false;
    size_t d0 = *(size_t *)bounds, d1 = *(size_t *)(bounds + 0x10);
    if (d0 == 0 || d1 == 0 || d0 > 4096 || d1 > 65536) return false;
    g.arr = arr;
    g.w = (int)d0;
    g.h = (int)d1;
    g.data = (float *)((uint8_t *)arr + 0x20);
    return true;
}

static Ref sOilGen, sOilData;
static FieldInfo *sGpSel, *sGpMapW, *sGpMapH, *sDrPattern, *sDrUnits, *sOgColor, *sPmData;
static int sOdOils = -2, sOdMatrix = -2, sOdSource = -2, sOdSrcAsset = -2, sOdName = -2, sOdId = -2, sOdDist = -2, sOdVol = -2;

static void *OilGenerator() {
    void *g = sOilGen.get();
    if (!g && N.tOilGen && sFrame % 60 == 0) { g = FirstAlive(FindAll(N.tOilGen)); sOilGen.set(g); }
    return g;
}

static bool OilList(ListView &lv) {
    void *d = sOilData.get();
    if (!d && N.tOilDescData) { d = FirstAlive(FindAllLoaded(N.tOilDescData)); sOilData.set(d); }
    if (!d) return false;
    if (sOdOils == -2) sOdOils = FieldOffset(ClassOf(d), "Oils");
    return sOdOils >= 0 && ReadList(At<void *>(d, sOdOils), lv) && lv.size > 0;
}

static void OilDescOffsets(void *desc) {
    if (sOdMatrix != -2 || !desc) return;
    Il2CppClass *k = ClassOf(desc);
    sOdMatrix = FieldOffset(k, "OilMatrix");
    sOdSource = FieldOffset(k, "_sourceOilMatrix");
    sOdSrcAsset = FieldOffset(k, "_source");
    sOdName = FieldOffset(k, "LongName");
    sOdId = FieldOffset(k, "Id");
    sOdDist = FieldOffset(k, "Distance");
    sOdVol = FieldOffset(k, "Volume");
}

static int OilSelected() { int v = 0; if (!sGpSel) sGpSel = StaticField(N.GameParams, "OIL_SELECTED"); if (sGpSel) StaticRead(sGpSel, &v); return v; }
static void SetOilSelected(int v) { if (!sGpSel) sGpSel = StaticField(N.GameParams, "OIL_SELECTED"); if (sGpSel) StaticWrite(sGpSel, &v); }

static void *OilDescAt(int index) {
    ListView lv;
    if (!OilList(lv) || index < 0 || index >= lv.size) return nullptr;
    void *d = lv.items[index];
    OilDescOffsets(d);
    return d;
}
static void *CurrentOilDesc() { return OilDescAt(OilSelected() - 1); }

static bool LiveIsClean(void *desc) {             // live grid == clean grid (fresh oil, e.g. a new game)
    OilGrid live, src;
    if (!desc || sOdMatrix < 0 || sOdSource < 0) return false;
    if (!GridOf(At<void *>(desc, sOdMatrix), live) || !GridOf(At<void *>(desc, sOdSource), src)) return false;
    return live.w == src.w && live.h == src.h && memcmp(live.data, src.data, sizeof(float) * live.w * live.h) == 0;
}

static bool InPractice() {
    return sSettled && gBFStatus.offline && sMode == MODE_FUN && !sInTutorial;
}

// For the oil features a shot replay is still part of the practice game. (gBFStatus.offline is false
// while LOC_REPLAYER is up, which made custom/invisible oil restore the original pattern for the replay.)
static bool InPracticeOil() {
    return sSettled && sMode == MODE_FUN && !sInTutorial && (gBFStatus.offline || sLoc == LOC_REPLAYER);
}

// ---- 1) mirror fix ----
static int sIdSizeX = 0, sIdOilMap = 0;
static int sMirrorLanes = 0;

// The flip needs the oil texture on Repeat, but the game makes each pattern's texture on Clamp when it
// first shows it (GenerateRG16Texture; later redraws reuse it). So: every frame we check the lane's
// current texture, and in the background we create every pattern's texture once
// (OilMapGenerator.GetTextureForId) and set it, so swiping the pattern carousel never flashes blank.
static void SetOilWrap(void *tex) {
    if (!Alive(tex) || !N.T_getWrap || !N.T_setWrap) return;
    int wrap = gBF.oilMirrorFix ? 0 : 1;          // 0 Repeat, 1 Clamp (the game's own setting)
    if (InvokeInt(N.T_getWrap, tex, nullptr, -1) == wrap) return;
    void *a[] = { &wrap };
    Invoke(N.T_setWrap, tex, a);
}

static int sPrewarmNext = 0, sPrewarmFor = -1, sPrewarmDone = 0;
static void OilPrewarmStep() {
    ListView lv;
    if (!N.OG_texForId || !OilList(lv)) return;
    if (sPrewarmFor != (int)gBF.oilMirrorFix) { sPrewarmFor = gBF.oilMirrorFix; sPrewarmNext = 0; sPrewarmDone = 0; }
    for (int n = 0; n < 6 && sPrewarmNext < lv.size; n++, sPrewarmNext++) {
        int idx = sPrewarmNext;                   // the picture cache is keyed by list position, not OilDescription.Id
        void *a[] = { &idx };
        void *tex = Invoke(N.OG_texForId, nullptr, a);
        if (Alive(tex)) { SetOilWrap(tex); sPrewarmDone++; }
    }
}

// The shot replay draws the recorded oil into its own texture (ReplayerInterfaceManager.manageOilOnLane,
// field oilTexture), made on Clamp, so it needs the same treatment.
static Ref sReplayer;
static void ReplayOilTick() {
    if (!N.tReplayer || N.ri_oilTex < 0) return;
    void *r = sReplayer.get();
    if (!r && sFrame % 60 == 0) { r = FirstAlive(FindAll(N.tReplayer)); sReplayer.set(r); }
    if (r) SetOilWrap(At<void *>(r, N.ri_oilTex));
}

// redraw one pattern's cached picture (carousel previews and the lane use these)
static void ReloadOilIndex(int idx) {
    void *gen = OilGenerator();
    if (!gen || idx < 0) return;
    if (N.OG_reloadById) { void *a[] = { &idx }; Invoke(N.OG_reloadById, gen, a); }
    else if (N.OG_reloadCurrent) Invoke(N.OG_reloadCurrent, gen, nullptr);
    if (N.OG_texForId) { void *a[] = { &idx }; SetOilWrap(Invoke(N.OG_texForId, nullptr, a)); }
}

static void *sLaneTex[16];
static int sIdOilHue = 0;
static float sHueShown = -1;                      // the hue BowlingPlus last put on the lane (-1 = game's own)

// Oil color: the lane shader only takes a hue (_OilHue; it uses full saturation and brightness).
static void ApplyHueTo(void *mat) {
    if (gBF.oilHue < 0 || !sIdOilHue || !N.M_getFloatI || !N.M_setFloatS) return;
    void *ga[] = { &sIdOilHue };
    bool ok = false;
    Il2CppObject *boxed = Invoke(N.M_getFloatI, mat, ga, &ok);
    float cur = (ok && boxed) ? *(float *)Unbox(boxed) : -2;
    if (fabsf(cur - gBF.oilHue) < 0.0005f) return;
    float v = gBF.oilHue;
    void *sa[] = { NewString("_OilHue"), &v };
    Invoke(N.M_setFloatS, mat, sa);
}

static void OilMirrorTick() {
    if (!sSettled || !N.R_getSharedMat || !N.M_hasProp || !N.M_getFloatI || !N.M_setFloatS) return;
    bool full = sFrame % 30 == 0;
    void *gen = OilGenerator();
    ListView lines;
    if (!gen || N.og_lines < 0 || !ReadList(At<void *>(gen, N.og_lines), lines)) return;
    if (!sIdSizeX && N.Sh_propToId) {
        void *a1[] = { NewString("_SizeX") };
        sIdSizeX = InvokeInt(N.Sh_propToId, nullptr, a1, 0);
        void *a2[] = { NewString("_OilMap") };
        sIdOilMap = InvokeInt(N.Sh_propToId, nullptr, a2, 0);
        void *a3[] = { NewString("_OilHue") };
        sIdOilHue = InvokeInt(N.Sh_propToId, nullptr, a3, 0);
    }
    if (!sIdOilMap) return;
    int lanes = 0;
    for (int i = 0; i < lines.size && i < 16; i++) {
        void *r = lines.items[i];
        if (!Alive(r)) continue;
        void *mat = Invoke(N.R_getSharedMat, r, nullptr);
        if (!Alive(mat)) continue;
        if (N.M_getTexI) {                         // every frame: a new texture on the lane gets Repeat at once
            void *ta[] = { &sIdOilMap };
            void *tex = Invoke(N.M_getTexI, mat, ta);
            if (tex != sLaneTex[i]) { sLaneTex[i] = tex; SetOilWrap(tex); }
        }
        ApplyHueTo(mat);                           // every frame, so the game can't put its own color back
        if (!full) { lanes++; continue; }
        void *ha[] = { NewString("_SizeX") };
        if (!InvokeBool(N.M_hasProp, mat, ha, false)) continue;
        void *ga[] = { &sIdSizeX };
        bool ok = false;
        Il2CppObject *boxed = Invoke(N.M_getFloatI, mat, ga, &ok);
        if (!ok || !boxed) continue;
        float v = *(float *)Unbox(boxed);
        float want = gBF.oilMirrorFix ? -fabsf(v) : fabsf(v);
        if (v != want && v != 0) {
            void *sa[] = { NewString("_SizeX"), &want };
            Invoke(N.M_setFloatS, mat, sa);
        }
        if (N.M_getTexI) {
            void *ta[] = { &sIdOilMap };
            SetOilWrap(Invoke(N.M_getTexI, mat, ta));
        }
        lanes++;
    }
    sMirrorLanes = lanes;
    if (gBF.oilHue < 0 && sHueShown >= 0) {       // back to the game's own color
        if (N.OG_updateColor) Invoke(N.OG_updateColor, gen, nullptr);
        sHueShown = -1;
    } else if (gBF.oilHue >= 0) sHueShown = gBF.oilHue;
    ReplayOilTick();
    if (full) OilPrewarmStep();
}

// ---- 2) show breakdown after every shot ----
static int sOilPrevLoc = -1;
static int sBreakdownRedraws = 0;

static void OilBreakdownTick() {
    int prev = sOilPrevLoc;
    sOilPrevLoc = sLoc;
    if (!gBF.oilBreakdown || gBF.oilInvisible || !InPracticeOil() || !N.OG_reloadCurrent) return;
    bool shotDone = (prev == LOC_THROWING || prev == LOC_ON_PINDECK || prev == LOC_REPLAYER) &&
                    (sLoc == LOC_START_POS || sLoc == LOC_BALL_RETURNER || sLoc == LOC_UPPER_SCREEN);
    if (!shotDone) return;
    void *gen = OilGenerator();
    if (!gen) return;
    Invoke(N.OG_reloadCurrent, gen, nullptr);     // redraws from the live grid if it changed
    sBreakdownRedraws++;
}

// ---- 3) invisible oil: hidden drawing + a random unlocked built-in pattern every game ----
static int sInvisOrig = -1, sInvisCur = -1, sInvisPicks = 0;
static bool sOilWasDirty = false;
static NSString *sInvisNote = @"";

static int RandomUnlockedOil(int avoid) {         // returns OIL_SELECTED value (1 + list index), or -1
    if (!sPmData) sPmData = StaticField(N.PracticeMgr, "_practiceData");
    void *pd = nullptr;
    if (sPmData) StaticRead(sPmData, &pd);
    static int oilDataOff = -2;
    if (pd && oilDataOff == -2) oilDataOff = FieldOffset(ClassOf(pd), "oil_data");
    Il2CppArray *arr = (pd && oilDataOff >= 0) ? At<Il2CppArray *>(pd, oilDataOff) : nullptr;
    ListView oils;
    if (!arr || !Len(arr) || !OilList(oils)) { sInvisNote = @"couldn't read your unlocked patterns"; return -1; }
    static int idOff = -2, stOff = -2;
    std::vector<int> picks;
    for (size_t i = 0; i < Len(arr); i++) {
        void *e = Elem(arr, i);
        if (!e) continue;
        if (idOff == -2) { idOff = FieldOffset(ClassOf(e), "oil_id"); stOff = FieldOffset(ClassOf(e), "state"); }
        if (idOff < 0 || stOff < 0 || At<int>(e, stOff) != 2) continue;    // EOilState.Unlocked
        int oilId = At<int>(e, idOff);
        for (int k = 0; k < oils.size; k++) {
            void *d = oils.items[k];
            OilDescOffsets(d);
            if (d && sOdId >= 0 && At<int>(d, sOdId) == oilId && k + 1 != avoid) { picks.push_back(k + 1); break; }
        }
    }
    if (picks.empty()) { sInvisNote = @"no other unlocked patterns"; return -1; }
    sInvisNote = [NSString stringWithFormat:@"picks from your %lu unlocked patterns", (unsigned long)picks.size() + 1];
    return picks[arc4random_uniform((uint32_t)picks.size())];
}

static void OilInvisibleTick() {
    void *gen = OilGenerator();
    bool want = gBF.oilInvisible && InPracticeOil();
    bool safe = sLoc == LOC_START_POS || sLoc == LOC_BALL_RETURNER || sLoc == LOC_UPPER_SCREEN;
    if (!want) {
        if (sInvisOrig > 0 && safe && gen && N.OG_reloadRedraw) {   // put the pattern you picked back
            SetOilSelected(sInvisOrig);
            Invoke(N.OG_reloadRedraw, gen, nullptr);
            BFLog(@"invisible oil: off, back to your pattern");
        }
        if (safe) sInvisOrig = sInvisCur = -1;
        return;
    }
    if (gen && N.OG_isActive && N.OG_showOnLane && InvokeBool(N.OG_isActive, gen, nullptr, false)) {
        bool off = false;
        void *a[] = { &off };
        Invoke(N.OG_showOnLane, gen, a);
    }
    if (!safe || !gen || !N.OG_reloadRedraw) return;
    int sel = OilSelected();
    bool clean = LiveIsClean(CurrentOilDesc());
    bool newGame = sOilWasDirty && clean;
    sOilWasDirty = !clean;
    if (sel == sInvisCur && !newGame) return;
    if (sel != sInvisCur) sInvisOrig = sel;        // the pattern the game / lobby set
    int pick = RandomUnlockedOil(sInvisOrig);
    if (pick <= 0) { sInvisCur = sel; return; }
    SetOilSelected(pick);
    Invoke(N.OG_reloadRedraw, gen, nullptr);       // fresh oil for that pattern
    bool off = false;
    void *a[] = { &off };
    if (N.OG_showOnLane) Invoke(N.OG_showOnLane, gen, a);
    sInvisCur = pick;
    sInvisPicks++;
    BFLog(@"invisible oil: random unlocked pattern for this game");
}

// ---- 4) custom patterns through the game's Kegel engine ----
static NSDictionary *sCustomPattern;              // set by the pattern library; nil = off
static NSString *sCustomAppliedId;
static ObjRef sCustomTarget;
static int sCustomTargetIdx = -1;               // its list position (the picture cache's key)
static NSDictionary *sLastKegel;                  // what the engine computed for the custom pattern (oil report)
static NSString *sOilReportSource;
static int sBuiltinPatched = 0;                   // how many of the game's own patterns are drawn from their Kegel files right now
static int sLastKegelBase = -1;
static std::vector<float> sCustomBackup;
static NSString *sCustomNote = @"";

static void *TemplateAsset(int index) {           // a real pattern file, for the machine settings
    void *d = OilDescAt(index);
    if (!d) d = OilDescAt(0);
    return (d && sOdSrcAsset >= 0) ? At<void *>(d, sOdSrcAsset) : nullptr;
}

static bool KegelSteps(void *pattern, NSArray *fwd, NSArray *rev) {
    static int fOff = -2, rOff = -2;
    if (fOff == -2) { fOff = FieldOffset(ClassOf(pattern), "mForward"); rOff = FieldOffset(ClassOf(pattern), "mReverse"); }
    void *lists[2] = { fOff >= 0 ? At<void *>(pattern, fOff) : nullptr, rOff >= 0 ? At<void *>(pattern, rOff) : nullptr };
    NSArray *steps[2] = { fwd, rev };
    for (int d = 0; d < 2; d++) {
        void *list = lists[d];
        if (!list) return false;
        const MethodInfo *clear = FindMethod(ClassOf(list), "Clear", 0), *add = FindMethod(ClassOf(list), "Add", 5);
        if (!clear || !add) return false;
        Invoke(clear, list, nullptr);
        for (NSArray *s in steps[d]) {
            if (s.count < 4) continue;
            int start = [s[0] intValue], stop = [s[1] intValue], loads = [s[2] intValue], speed = [s[3] intValue];
            // the engine works out a step's end from loads x speed; zero-load (travel only) steps use this
            float ef = s.count > 4 ? [s[4] floatValue] : 0;
            void *a[] = { &start, &stop, &loads, &speed, &ef };
            Invoke(add, list, a);
        }
    }
    return true;
}

static NSArray *KegelReadSteps(void *list) {      // [[start, stop, loads, speed, endFeet], ...]
    ListView lv;
    NSMutableArray *out = [NSMutableArray array];
    if (!list || !ReadList(list, lv)) return out;
    static int sO = -2, eO, lO, pO;
    static const MethodInfo *ef = nullptr;
    for (int i = 0; i < lv.size; i++) {
        void *ls = lv.items[i];
        if (!ls) continue;
        if (sO == -2) {
            Il2CppClass *k = ClassOf(ls);
            sO = FieldOffset(k, "mStart"); eO = FieldOffset(k, "mStop"); lO = FieldOffset(k, "mLoads"); pO = FieldOffset(k, "mSpeed");
            ef = FindMethod(k, "get_EndFootage", 0);
        }
        if (sO < 0) break;
        bool ok = false;
        Il2CppObject *f = ef ? Invoke(ef, ls, nullptr, &ok) : nullptr;
        [out addObject:@[ @(At<int>(ls, sO)), @(At<int>(ls, eO)), @(At<int>(ls, lO)), @(At<int>(ls, pO)),
                          @(ok && f ? *(float *)Unbox(f) : 0.0f) ]];
    }
    return out;
}

// Runs the game's Kegel engine. With steps == nil, returns the template pattern's own steps.
// drop = the sheet's "Reverse Brush Drop" in feet (Pattern.mTravel, 0xC4). Graph3DReverse only lays a
// reverse step's oil if that step ends short of it, so a custom pattern must not inherit the template's
// value. 0 = keep the template's.
static NSDictionary *KegelRun(int templateIndex, NSArray *fwd, NSArray *rev, int drop) {
    if (!N.D_drawFromFile || !N.D_graphF || !N.D_graphR || !N.Drawer) return nil;
    void *asset = TemplateAsset(templateIndex);
    if (!asset) return nil;
    void *a[] = { asset };
    bool ok = false;
    Invoke(N.D_drawFromFile, nullptr, a, &ok);
    if (!ok) return nil;
    void *pattern = ReadStaticObj(sDrPattern, N.Drawer, "mPattern");
    if (!pattern) return nil;
    static int travOff = -2;
    if (travOff == -2) travOff = FieldOffset(ClassOf(pattern), "mTravel");
    int templateDrop = travOff >= 0 ? At<int>(pattern, travOff) : -1;
    if ((fwd || rev) && drop > 0 && travOff >= 0) At<int>(pattern, travOff) = drop;
    int usedDrop = travOff >= 0 ? At<int>(pattern, travOff) : -1;
    if (fwd || rev) {
        if (!KegelSteps(pattern, fwd ?: @[], rev ?: @[])) return nil;
        OilGrid u;
        if (!GridOf(ReadStaticObj(sDrUnits, N.Drawer, "Units"), u)) return nil;
        memset(u.data, 0, sizeof(float) * u.w * u.h);
        Invoke(N.D_graphF, nullptr, nullptr, &ok);
        if (!ok) return nil;
        Invoke(N.D_graphR, nullptr, nullptr, &ok);
        if (!ok) return nil;
    }
    OilGrid u;
    if (!GridOf(ReadStaticObj(sDrUnits, N.Drawer, "Units"), u)) return nil;
    int w = 0, h = 0;
    if (!sGpMapW) sGpMapW = StaticField(N.GameParams, "OIL_MAP_WIDTH");
    if (!sGpMapH) sGpMapH = StaticField(N.GameParams, "OIL_MAP_LENGTH");
    if (sGpMapW) StaticRead(sGpMapW, &w);
    if (sGpMapH) StaticRead(sGpMapH, &h);
    if (w <= 0 || h <= 0 || w > 1024 || h > 16384) return nil;
    NSMutableData *grid = [NSMutableData dataWithLength:sizeof(float) * w * h];
    float *g = (float *)grid.mutableBytes, maxv = 0, sum = 0;
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++) {               // OilDescription.Parce: OilMatrix[w,h] = Units[w + 1, h / 4]
            int ux = x + 1, uy = y / 4;
            float v = (ux < u.w && uy < u.h) ? u.data[(size_t)ux * u.h + uy] : 0;
            g[(size_t)x * h + y] = v;
            maxv = fmaxf(maxv, v);
            sum += v;
        }
    void *fl = nullptr, *rl = nullptr;
    static int fOff = -2, rOff = -2;
    if (fOff == -2) { fOff = FieldOffset(ClassOf(pattern), "mForward"); rOff = FieldOffset(ClassOf(pattern), "mReverse"); }
    if (fOff >= 0) fl = At<void *>(pattern, fOff);
    if (rOff >= 0) rl = At<void *>(pattern, rOff);
    return @{ @"w": @(w), @"h": @(h), @"grid": grid, @"max": @(maxv), @"sum": @(sum),
              @"fwd": KegelReadSteps(fl), @"rev": KegelReadSteps(rl), @"drop": @(usedDrop), @"tdrop": @(templateDrop) };
}

// ---- BowlingPlus's copy of the game's Kegel drawing (Kegel.Drawer.Graph3DForward / Graph3DReverse) ----
// Decoded instruction by instruction and checked against the game on device (1.4.1 oil report: every
// sampled cell matched). Custom patterns are drawn with this instead of going through the game's
// PatternLoadScreens.Add, which rejects forward travel (zero-load) steps and places the first reverse
// step at a template setting (Pattern 0xC8), so sheets came out wrong. Here every step's distance is
// the sheet's (exact patterns) or the engine's own "new math" (patterns made in the editor).
struct KStep { int start, stop, loads, speed; float end; float ul = 50; };   // ul: the step's microliters per board

static int NetRound(double x) {                   // .NET Math.Round: halves go to the even number
    double f = floor(x), d = x - f;
    if (d == 0.5) return ((long long)f % 2 == 0) ? (int)f : (int)f + 1;
    return (int)floor(x + 0.5);
}

// Step ends: exact = the sheet's "End" column; otherwise LoadScreen.get_EndFootage's new math.
static void KegelEnds(NSArray *fwdIn, NSArray *revIn, bool exact, std::vector<KStep> &fwd, std::vector<KStep> &rev) {
    float prev = 0;
    bool first = true;
    for (int d = 0; d < 2; d++) {
        for (NSArray *a in (d == 0 ? fwdIn : revIn)) {
            if (a.count < 4) continue;
            KStep k = { [a[0] intValue], [a[1] intValue], [a[2] intValue], [a[3] intValue], a.count > 4 ? [a[4] floatValue] : 0,
                        a.count > 5 && [a[5] floatValue] > 0 ? [a[5] floatValue] : 50.f };
            if (k.speed <= 0) continue;
            if (!exact && k.loads != 0) {
                float dist = k.loads * k.speed * 17.f / 120.f;
                if (d == 0) k.end = first ? (k.loads - 1) * k.speed * 17.f / 120.f : prev + dist;
                else { k.end = prev - dist; if (k.end < 1.f) k.end = 0; }
            }
            prev = k.end;
            first = false;
            (d == 0 ? fwd : rev).push_back(k);
        }
    }
}

// fillGaps (BowlingPlus improvement, custom patterns only): the game only carries oil past a board's
// LAST oiled foot. A pattern that skips a board and comes back to it (2026 PBA Regional 37: 7L-7R, then
// 4L-9R; Kegel's "Start 5 and Stop 15" calibration pattern: alternating 2 ft bars) leaves it bare in
// between. Kegel draws those feet as buffed, a thin film. They get the same film the game's own transfer
// ends at (the board's loads so far x 1.5), never more than the oil on either side, before the reverse pass.
static void KegelDraw(const std::vector<KStep> &fwd, const std::vector<KStep> &rev, int travel, float U[41][60], bool fillGaps) {
    memset(U, 0, sizeof(float) * 41 * 60);
    float spd[61] = {}, buff[39] = {};
    int last[39] = {}, loads[39] = {};
    static bool cov[41][60];
    static int lsum[41][60];                       // loads a board has had once the oil head covers this foot
    memset(cov, 0, sizeof(cov));
    memset(lsum, 0, sizeof(lsum));
    auto put = [&](int b, int r, float v) { if (b >= 0 && b < 41 && r >= 0 && r < 60) U[b][r] = v; };
    auto get = [&](int b, int r) -> float { return (b >= 0 && b < 41 && r >= 0 && r < 60) ? U[b][r] : 0.f; };
    // forward
    int cur = 0, prevEnd = -1, end = 0;
    for (const KStep &k : fwd) {
        end = NetRound(k.end);
        float sp = (float)k.speed;
        if (k.loads != 0) {
            if (cur <= end) {
                for (int r = cur; r <= end; r++) {
                    if (r <= 60) spd[r] = sp;
                    for (int b = k.start; b <= k.stop; b++) {
                        put(b, r, 339.f / sp);
                        if (b >= 0 && b < 41 && r >= 0 && r < 60) {
                            cov[b][r] = true;
                            lsum[b][r] = (b < 39 ? loads[b] : 0) + k.loads;
                        }
                        if (r == end && b >= 0 && b < 39) { last[b] = end; loads[b] += k.loads; }
                    }
                }
                for (int b = 2; b <= 38; b++)       // boards this step doesn't oil keep building machine time
                    if (b < k.start || b > k.stop) buff[b] += (float)(end - prevEnd) / sp;
            }
        } else {                                  // travel: no oil, machine time for every board
            for (int b = 2; b <= 38; b++) {
                buff[b] += (float)(end - prevEnd) / sp;
                if (last[b] == 0) last[b] = cur;
            }
            for (int r = cur; r <= end && r <= 60; r++) spd[r] = sp;
        }
        cur = end + 1;
        prevEnd = end;
    }
    // transfer brush: each board fades from its last oiled foot to loads x 1.5 at the pattern's end
    int endRow = end;
    for (int b = 2; b <= 38; b++) {
        put(b, endRow, loads[b] * 1.5f);
        float slope = buff[b] != 0 ? (get(b, endRow) - get(b, last[b])) / buff[b] : 0.f;
        for (int r = last[b] + 1; r < endRow; r++) put(b, r, get(b, r - 1) + (spd[r] > 0 ? slope / spd[r] : 0.f));
    }
    if (fillGaps) {
        for (int b = 2; b <= 38; b++) {
            int p = -1;
            for (int r = 0; r <= endRow && r < 60; r++) {
                if (!cov[b][r]) continue;
                if (p >= 0 && r - p > 1) {            // feet p+1 .. r-1 were skipped by the oil head
                    float film = fminf(lsum[b][p] * 1.5f, fminf(U[b][p], U[b][r]));
                    for (int q = p + 1; q < r; q++) if (U[b][q] == 0) U[b][q] = film;
                }
                p = r;
            }
        }
    }
    // reverse: new = 2 x old + 339 / speed; only steps ending short of the reverse brush drop pick
    // their own boards (others keep the previous step's); a travel step to the foul line still oils
    int bs = 0, be = 0, row = 0;
    for (const KStep &k : rev) {
        int e = NetRound(k.end);
        float add = 339.f / (float)k.speed;
        if (k.loads >= 1) {
            if (e < travel) { bs = k.start; be = k.stop; }
            if (row >= e && bs <= be)
                for (int r = row; r >= e; r--) for (int b = bs; b <= be; b++) put(b, r, 2 * get(b, r) + add);
            row = e - 1;
        } else if (k.loads == 0 && e >= 1) {
            row = e - 1;
        } else if (e == 0) {
            if (row >= 0 && bs <= be)
                for (int r = row; r >= 0; r--) for (int b = bs; b <= be; b++) put(b, r, 2 * get(b, r) + add);
        } else {
            row = e - 1;
        }
    }
}

// custom = a BowlingPlus pattern: fill skipped gaps, and put Kegel's left boards on the bowler's left.
// The game's own grids have lane column 0 on the bowler's RIGHT (OilMatrix[w] = Units[w + 1]), so its
// Kegel patterns are mirrored; device screenshots of 2026 PBA Regional 37 (lopsided: 2L-6R, 4L-9R)
// showed the left-side features on the right. custom = false reproduces the game exactly (self-check).
// ---- Kegel-accurate oil, v2 (custom patterns, "Kegel-accurate oil" on) ----
// Everything below was measured from Kegel's own charts and files (2025 U.S. Open #4, 2017 SEA Games Long,
// 2026 Regional 37, the Start 5 / Stop 15 calibration pattern; see VERIFIED_NOTES.md "Kegel's chart"):
//  * Geometry is continuous. A load travels speed x 0.14 ft (14 in/s = 1.96 ft per load), the first step of
//    the forward pass counts loads - 1, and a step covers exactly [previous end, end]. The sheet's whole-foot
//    "10 -> 14" is that rounded for display (it is 9.80 -> 13.72). The lane's oil map has 4 rows per foot, so
//    edges land within 0.125 ft and a partly covered row gets its share of the oil.
//  * Oil per foot per board = 339 / speed x uL / 50 (the game's density, Kegel's own for 50 uL), added for
//    every pass over a cell. Reverse oil counts only below the reverse brush drop.
//  * The final travel to the foul line carries the last loaded reverse step's oil down the front of the lane
//    (over that step's boards), like the brush on a real machine.
//  * A brushed film covers EVERY board from 2 to 38 (also boards the oil head never crossed) from the foul
//    line to the pattern distance: strongest at the foul line, fading linearly, then a lighter tier from the
//    reverse brush drop to the distance. Kegel's chart draws exactly this under the passes.
//  * No scaling to the game's total: the units are the game's own single-pass units (a 50 uL pass at 14 in/s
//    is 24), so the oil amounts are the sheet's, not inflated.
static const float kFilmFront = 25.f;           // film at the foul line, in game oil units (about one 50 uL pass)
static float KegelDensity(const KStep &k) { return k.speed > 0 ? 339.f / k.speed * (k.ul / 50.f) : 0.f; }

// Step ends from the sheet's loads and speeds. precise: the given ends are exact (Kegel's .Pattern file, the
// collection): use them as they are. Otherwise (PDF / text / editor) the loaded steps are recomputed with
// Kegel's rule and only the travel destinations (zero-load steps) are taken from the sheet.
static void KegelExactChainV(const std::vector<KStep> &fwdIn, const std::vector<KStep> &revIn, int drop, bool precise,
                             std::vector<KStep> &fwd, std::vector<KStep> &rev) {
    float pos = 0;
    bool first = true;
    for (KStep k : fwdIn) {
        if (k.speed <= 0) continue;
        if (k.loads > 0 && !precise) k.end = pos + fmaxf(0, (float)(first ? k.loads - 1 : k.loads)) * k.speed * 0.14f;
        if (k.end < pos) k.end = pos;              // oil never goes back up the lane in the forward pass
        pos = k.end;
        first = false;
        fwd.push_back(k);
    }
    float rpos = 0;
    bool started = false;
    for (KStep k : revIn) {
        if (k.speed <= 0) continue;
        if (k.loads > 0) {
            if (!started) { rpos = drop > 0 ? (float)drop : pos; started = true; }
            if (!precise) k.end = rpos - k.loads * k.speed * 0.14f;
        } else if (started && k.end > rpos) k.end = rpos;   // a travel never goes back up either
        if (k.end < 0) k.end = 0;
        started = true;
        rpos = k.end;
        rev.push_back(k);
    }
}

static std::vector<KStep> KegelParseSteps(NSArray *in) {
    std::vector<KStep> out;
    for (NSArray *a in in) {
        if (a.count < 4) continue;
        out.push_back({ [a[0] intValue], [a[1] intValue], [a[2] intValue], [a[3] intValue], a.count > 4 ? [a[4] floatValue] : 0,
                        a.count > 5 && [a[5] floatValue] > 0 ? [a[5] floatValue] : 50.f });
    }
    return out;
}

static void KegelDrawExactK(const std::vector<KStep> &fwd, const std::vector<KStep> &rev, int drop, int feet, float V[41][240]) {
    memset(V, 0, sizeof(float) * 41 * 240);
    float lastFwd = 0;
    for (const KStep &k : fwd) lastFwd = fmaxf(lastFwd, k.end);
    float dist = feet > 0 ? (float)feet : ceilf(lastFwd);
    if (dist > 60.f) dist = 60.f;
    float dropEff = (drop > 0 && drop < dist) ? (float)drop : dist;
    // [boards b0..b1] x [ft a..b] += v, each 0.25 ft row by the share of it the interval covers
    auto addRect = [&](int b0, int b1, float a, float b, float v) {
        a = fmaxf(a, 0.f);
        b = fminf(b, 60.f);
        if (b <= a || v == 0) return;
        int r0 = (int)floorf(a * 4.f), r1 = (int)ceilf(b * 4.f) - 1;
        for (int r = r0; r <= r1 && r < 240; r++) {
            float lo = fmaxf(a, r / 4.f), hi = fminf(b, (r + 1) / 4.f);
            float cov = (hi - lo) * 4.f;
            if (cov <= 0) continue;
            for (int bd = b0; bd <= b1; bd++) if (bd >= 1 && bd <= 39) V[bd][r] += v * cov;
        }
    };
    // the brushed film, boards 2..38 (Kegel's chart: strongest at the foul line, lighter past the brush drop)
    auto film = [&](float ft) -> float {
        float fa = kFilmFront * (1.f - 0.01177f * ft);
        if (ft < dropEff) return fa;
        float atDrop = 0.6f * kFilmFront * (1.f - 0.01177f * dropEff), atEnd = 0.27f * kFilmFront;
        return dist > dropEff ? atDrop + (atEnd - atDrop) * (ft - dropEff) / (dist - dropEff) : atDrop;
    };
    for (int r = 0; r < 240; r++) {
        float ft = (r + 0.5f) / 4.f;
        if (ft >= dist) break;
        float f = film(ft);
        for (int bd = 2; bd <= 38; bd++) V[bd][r] += f;
    }
    float prev = 0;
    for (const KStep &k : fwd) {                   // forward passes: [previous end, end]
        if (k.loads > 0) addRect(k.start, k.stop, prev, k.end, KegelDensity(k));
        prev = k.end;
    }
    float p = drop > 0 ? (float)drop : dist;       // reverse passes: [end, previous end], below the brush drop
    bool haveLast = false;
    int lb0 = 0, lb1 = -1;
    float ldens = 0;
    for (const KStep &k : rev) {
        if (k.loads > 0) {
            float hi = drop > 0 ? fminf(p, (float)drop) : p;
            addRect(k.start, k.stop, k.end, hi, KegelDensity(k));
            haveLast = true; lb0 = k.start; lb1 = k.stop; ldens = KegelDensity(k);
        } else if (k.end < 0.5f && haveLast) {     // the last travel to the foul line: the brush carries the last load down
            addRect(lb0, lb1, 0.f, p, ldens);
        }
        p = k.end;
    }
}

static NSDictionary *KegelDrawPattern(NSArray *fwdIn, NSArray *revIn, int drop, bool exact, bool custom, int feet = 0, bool precise = false) {
    std::vector<KStep> fwd, rev;
    bool kegel2 = custom;                          // BowlingPlus patterns always use the Kegel-accurate model
    static float U[41][60], V[41][240];
    if (kegel2) {                                  // Kegel-accurate v2: exact distances, 0.25 ft rows, full-lane film
        KegelExactChainV(KegelParseSteps(fwdIn), KegelParseSteps(revIn), drop, precise, fwd, rev);
        KegelDrawExactK(fwd, rev, drop, feet, V);
    } else {                                       // the game's own model
        KegelEnds(fwdIn, revIn, exact, fwd, rev);
        KegelDraw(fwd, rev, drop > 0 ? drop : 60, U, custom);
    }
    int w = 0, h = 0;
    if (!sGpMapW) sGpMapW = StaticField(N.GameParams, "OIL_MAP_WIDTH");
    if (!sGpMapH) sGpMapH = StaticField(N.GameParams, "OIL_MAP_LENGTH");
    if (sGpMapW) StaticRead(sGpMapW, &w);
    if (sGpMapH) StaticRead(sGpMapH, &h);
    if (w <= 0 || h <= 0 || w > 1024 || h > 16384) { w = 39; h = 240; }
    NSMutableData *grid = [NSMutableData dataWithLength:sizeof(float) * w * h];
    float *g = (float *)grid.mutableBytes, maxv = 0, sum = 0;
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++) {               // like OilDescription.Parce: OilMatrix[w,h] = Units[w + 1, h / 4]
            int board = custom ? w - x : x + 1;
            float v = 0;
            if (board >= 0 && board < 41) {
                if (kegel2) v = V[board][y * 240 / h];            // the map's rows are the model's rows (4 per foot)
                else if (y / 4 < 60) v = U[board][y / 4];
            }
            g[(size_t)x * h + y] = v;
            maxv = fmaxf(maxv, v);
            sum += v;
        }
    NSMutableArray *fo = [NSMutableArray array], *ro = [NSMutableArray array];
    for (const KStep &k : fwd) [fo addObject:@[ @(k.start), @(k.stop), @(k.loads), @(k.speed), @(k.end), @(k.ul) ]];
    for (const KStep &k : rev) [ro addObject:@[ @(k.start), @(k.stop), @(k.loads), @(k.speed), @(k.end), @(k.ul) ]];
    return @{ @"w": @(w), @"h": @(h), @"grid": grid, @"max": @(maxv), @"sum": @(sum), @"fwd": fo, @"rev": ro,
              @"drop": @(drop > 0 ? drop : 60), @"tdrop": @(drop), @"feet": @(feet) };
}

// Engine self-check for the oil report: draw one of the game's own patterns with our copy (its steps and
// brush drop as the game reads them) and compare with the game's own drawing.
static NSString *KegelSelfCheck(int index) {
    NSDictionary *game = KegelRun(index, nil, nil, 0);
    if (!game) return @"self-check: couldn't read the pattern";
    NSDictionary *ours = KegelDrawPattern(game[@"fwd"], game[@"rev"], [game[@"tdrop"] intValue], true, false);
    NSData *a = game[@"grid"], *b = ours[@"grid"];
    if (a.length != b.length) return @"self-check: size mismatch";
    const float *x = (const float *)a.bytes, *y = (const float *)b.bytes;
    size_t n = a.length / sizeof(float), bad = 0;
    float worst = 0;
    for (size_t i = 0; i < n; i++) { float d = fabsf(x[i] - y[i]); if (d > 0.05f) bad++; worst = fmaxf(worst, d); }
    return [NSString stringWithFormat:@"self-check vs game's own drawing of pattern %d: max diff %.3f, %zu of %zu cells differ",
            index + 1, worst, bad, n];
}

static void RestoreCustom() {
    void *d = sCustomTarget.get();
    OilGrid src, live;
    if (d && sOdSource >= 0 && GridOf(At<void *>(d, sOdSource), src) && (size_t)src.w * src.h == sCustomBackup.size()) {
        memcpy(src.data, sCustomBackup.data(), sizeof(float) * sCustomBackup.size());
        if (sOdMatrix >= 0 && GridOf(At<void *>(d, sOdMatrix), live) && live.w == src.w && live.h == src.h)
            memcpy(live.data, src.data, sizeof(float) * sCustomBackup.size());
        ReloadOilIndex(sCustomTargetIdx);         // redraw THAT pattern's picture, not whichever is current
        BFLog(@"custom oil: the original pattern is back");
    }
    sCustomTarget.set(nullptr);
    sCustomTargetIdx = -1;
    sCustomBackup.clear();
    sCustomAppliedId = nil;
}

static void OilCustomTick() {
    bool want = sCustomPattern && InPracticeOil() && !gBF.oilInvisible;
    void *cur = CurrentOilDesc();
    bool safe = sLoc == LOC_START_POS || sLoc == LOC_BALL_RETURNER || sLoc == LOC_UPPER_SCREEN;
    NSString *pid = sCustomPattern[@"id"];
    if (sCustomAppliedId && (!want || cur != sCustomTarget.get() || ![pid isEqualToString:sCustomAppliedId])) {
        if (!want || safe) RestoreCustom();
    }
    if (!want || sCustomAppliedId || !cur || !safe || sOdSource < 0 || sOdMatrix < 0) return;
    OilGrid src, live;
    if (!GridOf(At<void *>(cur, sOdSource), src) || !GridOf(At<void *>(cur, sOdMatrix), live) || src.w != live.w || src.h != live.h) return;
    // Build from the pattern's own starting template, never from the pattern it replaces: the template's
    // machine settings would leak in and the same custom pattern would look different on each host.
    ListView ol;
    int base = [sCustomPattern[@"base"] intValue];
    if (!OilList(ol) || base < 0 || base >= ol.size) base = 0;
    NSDictionary *r = KegelDrawPattern(sCustomPattern[@"fwd"], sCustomPattern[@"rev"], [sCustomPattern[@"drop"] intValue],
                                       [sCustomPattern[@"exact"] boolValue], true,
                                       [sCustomPattern[@"feet"] intValue], [sCustomPattern[@"precise"] boolValue]);
    sLastKegel = r;
    sLastKegelBase = base;
    NSData *grid = r[@"grid"];
    if ([r[@"w"] intValue] != src.w || [r[@"h"] intValue] != src.h || grid.length != sizeof(float) * src.w * src.h) {
        sCustomNote = @"couldn't build the pattern";
        sCustomAppliedId = pid;      // don't retry every tick
        return;
    }
    sCustomBackup.assign(src.data, src.data + (size_t)src.w * src.h);
    memcpy(src.data, grid.bytes, grid.length);    // a new game copies this clean grid back, so it stays
    memcpy(live.data, grid.bytes, grid.length);
    sCustomTarget.set(cur);
    sCustomTargetIdx = OilSelected() - 1;
    sCustomAppliedId = pid;
    sCustomNote = @"";
    ReloadOilIndex(sCustomTargetIdx);
    BFLog(@"custom oil: \"%@\" is on the lane", sCustomPattern[@"name"]);
}

// ---- 4b) the game's own patterns, drawn like real life (Practice) ----
// The game builds its 48 patterns with its simplified Kegel engine (whole-foot rows, reverse oil doubling,
// no film on boards the oil head never crossed, left and right mirrored). Each pattern still carries its
// real Kegel file (OilDescription._source, a TextAsset), so in Practice the lane's oil grids are replaced by
// the Kegel-accurate drawing of that file: exact distances on the lane's quarter-foot rows, microliters,
// the brushed film, and Kegel's left on the bowler's left. All 48 are swapped at once (so the pattern
// carousel shows them right away), the originals are kept, and everything is put back the moment you're not
// in Practice (online matches, tournaments, the tutorial always get the game's own oil).
static NSMutableDictionary<NSNumber *, NSDictionary *> *sBuiltinSpec;
static struct BuiltinState { void *desc = nullptr; bool applied = false; std::vector<float> backup; } sBI[64];
static uint64_t sBuiltinRedraw = 0;               // patterns whose lane picture still needs redrawing
static int sBuiltinFails = 0;

static NSDictionary *BuiltinSpec(int idx, void *desc) {
    if (idx < 0 || idx >= 64 || !desc) return nil;
    if (!sBuiltinSpec) sBuiltinSpec = [NSMutableDictionary dictionary];
    NSDictionary *c = sBuiltinSpec[@(idx)];
    if (c) return c.count ? c : nil;
    OilDescOffsets(desc);
    NSDictionary *spec = nil;
    void *ta = sOdSrcAsset >= 0 ? At<void *>(desc, sOdSrcAsset) : nullptr;
    if (Alive(ta) && N.TA_getText) {
        NSString *text = Str(Invoke(N.TA_getText, ta, nullptr));
        if (text.length) {
            std::vector<std::string> lines;
            for (NSString *l in [text componentsSeparatedByString:@"\n"]) lines.push_back(l.UTF8String ?: "");
            KegelFile f = KegelParseLines(lines);
            if (f.ok) {
                NSMutableArray *fw = [NSMutableArray array], *rv = [NSMutableArray array];
                for (const KegelFileStep &k : f.fwd) [fw addObject:@[ @(k.start), @(k.stop), @(k.loads), @(k.speed), @(k.end), @(f.ul) ]];
                for (const KegelFileStep &k : f.rev) [rv addObject:@[ @(k.start), @(k.stop), @(k.loads), @(k.speed), @(k.end), @(f.ul) ]];
                NSString *nm = sOdName >= 0 ? Str(At<void *>(desc, sOdName)) : nil;
                spec = @{ @"fwd": fw, @"rev": rv, @"drop": @(f.drop), @"feet": @(f.feet), @"ul": @(f.ul), @"name": nm ?: @"pattern" };
            }
        }
    }
    sBuiltinSpec[@(idx)] = spec ?: @{};
    if (!spec) { sBuiltinFails++; BFLog(@"built-in oil pattern %d: couldn't read its Kegel file, it keeps the game's drawing", idx + 1); }
    return spec;
}

NSDictionary *BFOilBuiltinSpec(int idx) {          // for the editor's "Start from"
    void *d = OilDescAt(idx);
    return d ? BuiltinSpec(idx, d) : nil;
}

static void BuiltinRestoreAll() {
    if (!sBuiltinPatched) { for (int i = 0; i < 64; i++) sBI[i] = BuiltinState(); return; }
    if (sCustomAppliedId) RestoreCustom();         // a custom pattern may sit on top of one of them: back to the patched first
    ListView lv;
    bool haveList = OilList(lv);
    for (int i = 0; i < 64; i++) {
        if (!sBI[i].applied) continue;
        if (haveList && i < lv.size && lv.items[i] == sBI[i].desc && sOdSource >= 0 && sOdMatrix >= 0) {
            void *d = lv.items[i];
            OilGrid src, live;
            if (GridOf(At<void *>(d, sOdSource), src) && (size_t)src.w * src.h == sBI[i].backup.size()) {
                memcpy(src.data, sBI[i].backup.data(), sizeof(float) * sBI[i].backup.size());
                if (GridOf(At<void *>(d, sOdMatrix), live) && live.w == src.w && live.h == src.h)
                    memcpy(live.data, src.data, sizeof(float) * sBI[i].backup.size());
                sBuiltinRedraw |= 1ull << i;
            }
        }
        sBI[i] = BuiltinState();
    }
    sBuiltinPatched = 0;
    BFLog(@"the game's own oil patterns are back (not in Practice)");
}

static void BuiltinApplyAll() {
    ListView lv;
    if (!OilList(lv)) return;
    int n = lv.size < 64 ? lv.size : 64, added = 0;
    for (int i = 0; i < n; i++) {
        void *d = lv.items[i];
        if (!d) continue;
        if (sBI[i].applied && sBI[i].desc == d) continue;
        if (sBI[i].applied) sBI[i] = BuiltinState();                 // the list changed under us
        if (sCustomAppliedId && i == sCustomTargetIdx) continue;     // a custom pattern is on this one right now
        OilDescOffsets(d);
        if (sOdSource < 0 || sOdMatrix < 0) return;
        NSDictionary *spec = BuiltinSpec(i, d);
        if (!spec) continue;
        OilGrid src, live;
        if (!GridOf(At<void *>(d, sOdSource), src) || !GridOf(At<void *>(d, sOdMatrix), live) || src.w != live.w || src.h != live.h) continue;
        NSDictionary *r = KegelDrawPattern(spec[@"fwd"], spec[@"rev"], [spec[@"drop"] intValue], true, true, [spec[@"feet"] intValue], true);
        NSData *grid = r[@"grid"];
        if ([r[@"w"] intValue] != src.w || [r[@"h"] intValue] != src.h || grid.length != sizeof(float) * src.w * src.h) continue;
        sBI[i].backup.assign(src.data, src.data + (size_t)src.w * src.h);
        memcpy(src.data, grid.bytes, grid.length);                    // a new game copies this clean grid back, so it stays
        memcpy(live.data, grid.bytes, grid.length);
        sBI[i].desc = d;
        sBI[i].applied = true;
        sBuiltinRedraw |= 1ull << i;
        added++;
    }
    int total = 0;
    for (int i = 0; i < 64; i++) total += sBI[i].applied;
    sBuiltinPatched = total;
    if (added) BFLog(@"Practice: %d of the game's oil patterns are now drawn from their Kegel files (%d couldn't be read)", total, sBuiltinFails);
}

static void BuiltinRedrawStep() {                 // lane pictures: the selected pattern first, then a few per call
    if (!sBuiltinRedraw) return;
    int budget = 6, cur = OilSelected() - 1;
    if (cur >= 0 && cur < 64 && ((sBuiltinRedraw >> cur) & 1)) { sBuiltinRedraw &= ~(1ull << cur); ReloadOilIndex(cur); budget--; }
    for (int i = 0; i < 64 && budget > 0; i++)
        if ((sBuiltinRedraw >> i) & 1) { sBuiltinRedraw &= ~(1ull << i); ReloadOilIndex(i); budget--; }
}

static void OilBuiltinTick() {
    bool want = InPracticeOil();
    if (!want) { if (sBuiltinPatched) BuiltinRestoreAll(); }
    else if (sFrame % 15 == 0 || !sBuiltinPatched) BuiltinApplyAll();
    if (sFrame % 3 == 0) BuiltinRedrawStep();
}

// ---- 5) show oil thickness (display only) ----
// GenerateRG16Texture bakes each cell's color from OilColorData: Gradient.Evaluate(oil / MaxHeight) ->
// RGBToHSV -> texture R = brightness, G = tint. The lane shader then does wood x mix(white, your oil
// color, G) x R. The game's gradient gives every cell above ~6 units the same tint (MaxHeight 100), so
// real patterns (about 10-70 units) all look one color. This swaps in a thickness scale over 0-75
// units (thicker = more tint and darker), redraws every pattern's picture, and puts the original back
// when turned off. Physics never reads the gradient.
static bool sThickSaved = false, sThickApplied = false;
static uint8_t sThickOrigKeys[8 * 20];
static int sThickOrigCount = 0, sThickRedrawNext = -1;
static float sThickOrigMax = 100;

static void HsvToRgb(float h, float s, float v, float out[3]) {
    float r = fabsf(h * 6 - 3) - 1, g = 2 - fabsf(h * 6 - 2), b = 2 - fabsf(h * 6 - 4);
    float c[3] = { fminf(fmaxf(r, 0), 1), fminf(fmaxf(g, 0), 1), fminf(fmaxf(b, 0), 1) };
    for (int i = 0; i < 3; i++) out[i] = v * (1 - s + s * c[i]);
}

static bool ThickApply(bool on) {
    void *cd = N.OG_colorData ? Invoke(N.OG_colorData, nullptr, nullptr) : nullptr;
    if (!Alive(cd) || !N.G_getKeys || !N.G_setKeys) return false;
    static int offG = -2, offM = -2;
    if (offG == -2) { offG = FieldOffset(ClassOf(cd), "OilGradient"); offM = FieldOffset(ClassOf(cd), "MaxHeight"); }
    if (offG < 0 || offM < 0) return false;
    void *grad = At<void *>(cd, offG);
    if (!grad) return false;
    Il2CppArray *keys = (Il2CppArray *)Invoke(N.G_getKeys, grad, nullptr);
    size_t n = Len(keys);
    if (n < 5 || n > 8) return false;             // this game's gradient has 5 color keys
    uint8_t *data = (uint8_t *)keys + 0x20;       // GradientColorKey { Color color; float time } = 20 bytes
    if (!sThickSaved) {
        memcpy(sThickOrigKeys, data, n * 20);
        sThickOrigCount = (int)n;
        sThickOrigMax = At<float>(cd, offM);
        sThickSaved = true;
    }
    if (on) {
        // { time, tint (S), brightness (V) }; hue doesn't matter, the shader uses your oil color
        static const float k[5][3] = { {0.00f, 0.00f, 1.00f}, {0.01f, 0.20f, 1.00f}, {0.25f, 0.45f, 0.97f},
                                       {0.55f, 0.75f, 0.85f}, {1.00f, 1.00f, 0.65f} };
        for (size_t i = 0; i < n; i++) {
            const float *kk = k[i < 5 ? i : 4];
            float rgb[3];
            HsvToRgb(0.6f, kk[1], kk[2], rgb);
            float *e = (float *)(data + i * 20);
            e[0] = rgb[0]; e[1] = rgb[1]; e[2] = rgb[2]; e[3] = 1; e[4] = kk[0];
        }
        At<float>(cd, offM) = 75.f;
    } else {
        if (sThickOrigCount != (int)n) return false;
        memcpy(data, sThickOrigKeys, n * 20);
        At<float>(cd, offM) = sThickOrigMax;
    }
    void *a[] = { keys };
    bool ok = false;
    Invoke(N.G_setKeys, grad, a, &ok);
    return ok;
}

static void ThickRedrawIndex(int idx) {          // redraw one pattern's cached picture with the current colors
    void *d = OilDescAt(idx);
    if (!d || !N.OG_genRG16 || !N.OG_texForId) return;
    OilDescOffsets(d);
    void *m = sOdMatrix >= 0 ? At<void *>(d, sOdMatrix) : nullptr;
    void *a1[] = { &idx };
    void *tex = Invoke(N.OG_texForId, nullptr, a1);
    if (!m || !Alive(tex)) return;
    void *a2[] = { m, &tex };                     // (float[,] oilMap, ref Texture2D tex): redrawn in place
    Invoke(N.OG_genRG16, nullptr, a2);
    SetOilWrap(tex);
}

static void OilThicknessTick() {                  // every 30 frames
    if (!sSettled) return;
    if (gBF.oilThickness != sThickApplied && ThickApply(gBF.oilThickness)) {
        sThickApplied = gBF.oilThickness;
        ThickRedrawIndex(OilSelected() - 1);      // the lane you're looking at first
        sThickRedrawNext = 0;                     // then every pattern, a few at a time
        BFLog(@"oil thickness colors %@", sThickApplied ? @"on" : @"off");
    }
    ListView lv;
    if (sThickRedrawNext < 0 || !OilList(lv)) return;
    for (int n = 0; n < 6 && sThickRedrawNext < lv.size; n++) ThickRedrawIndex(sThickRedrawNext++);
    if (sThickRedrawNext >= lv.size) sThickRedrawNext = -1;
}

// "Copy debug info" oil report: what the engine computed for the custom pattern next to what is on the
// lane right now, sampled every 5 ft on an outside (3), mid (10) and center (20) board.
static NSString *OilReport(bool live) {
    NSMutableString *s = [NSMutableString string];
    NSDictionary *r = sLastKegel;
    sOilReportSource = sCustomAppliedId ? @"custom pattern, Kegel-accurate model" : @"";
    if (!sCustomAppliedId && live && sBuiltinPatched) {          // the game's own pattern, drawn from its Kegel file
        int idx = OilSelected() - 1;
        void *dd = CurrentOilDesc();
        NSDictionary *spec = (dd && idx >= 0) ? BuiltinSpec(idx, dd) : nil;
        if (spec) {
            r = KegelDrawPattern(spec[@"fwd"], spec[@"rev"], [spec[@"drop"] intValue], true, true, [spec[@"feet"] intValue], true);
            sOilReportSource = [NSString stringWithFormat:@"game pattern \"%@\" from its Kegel file, Kegel-accurate model (%d of 48 patterns patched)", spec[@"name"], sBuiltinPatched];
        }
    }
    int w = [r[@"w"] intValue], h = [r[@"h"] intValue];
    const float *eg = (const float *)[r[@"grid"] bytes];
    OilGrid lane = {};
    void *d = live ? CurrentOilDesc() : nullptr;
    if (d) { OilDescOffsets(d); if (sOdMatrix >= 0) GridOf(At<void *>(d, sOdMatrix), lane); }
    if (!r && !lane.data) return @"";
    int lw = lane.data ? lane.w : w, lh = lane.data ? lane.h : h;
    [s appendFormat:@"oil report: map %dx%d (%.2f rows/ft) | custom template #%d brush drop used %@ (template %@) | %@ | lane = pattern %d\n",
        lw, lh, lh / 60.0, sLastKegelBase, r[@"drop"] ?: @"-", r[@"tdrop"] ?: @"-", sOilReportSource ?: @"", live ? OilSelected() : -1];
    NSMutableArray *fe = [NSMutableArray array], *re = [NSMutableArray array];
    for (NSArray *st in r[@"fwd"]) [fe addObject:[NSString stringWithFormat:@"%.1f", [st[4] floatValue]]];
    for (NSArray *st in r[@"rev"]) [re addObject:[NSString stringWithFormat:@"%.1f", [st[4] floatValue]]];
    if (r) [s appendFormat:@"  engine step ends: fwd %@ | rev %@\n", [fe componentsJoinedByString:@" "], [re componentsJoinedByString:@" "]];
    if (live) [s appendFormat:@"  %@\n", KegelSelfCheck(OilSelected() - 1)];
    [s appendString:@"  ft: engine c3/c10/c20 | lane c3/c10/c20 (lane columns, 1 = bowler's right)\n"];
    int b[3] = { 3, 10, 20 };
    for (int ft = 0; ft <= 45; ft += 5) {
        [s appendFormat:@"  %2d:", ft];
        for (int k = 0; k < 3; k++) {
            int x = b[k] - 1, y = (int)((ft + 0.5) * h / 60.0);
            [s appendFormat:@" %5.1f", (eg && x < w && y < h) ? eg[(size_t)x * h + y] : -1.f];
        }
        [s appendString:@" |"];
        for (int k = 0; k < 3; k++) {
            int x = b[k] - 1, y = (int)((ft + 0.5) * lh / 60.0);
            [s appendFormat:@" %5.1f", (lane.data && x < lane.w && y < lane.h) ? lane.data[(size_t)x * lane.h + y] : -1.f];
        }
        [s appendString:@"\n"];
    }
    return s;
}

void BFOilApplyHue(void) {                         // called by the color picker: the next frame shows it
    if (gBF.oilHue >= 0) sHueShown = gBF.oilHue;
}

static void OilTick() {
    OilBreakdownTick();                           // every frame (it watches for the end of a shot)
    OilMirrorTick();                              // every frame (cheap texture check), full pass every 30
    if (sSettled && N.ok) OilBuiltinTick();       // every frame: leaving Practice must put the game's oil back at once
    if (sFrame % 30 != 0) return;
    OilThicknessTick();
    OilInvisibleTick();
    OilCustomTick();
}

// ---- for the pattern library UI ----
bool BFOilReady(void) { ListView lv; return N.ok && sSettled && OilList(lv); }

NSArray<NSDictionary *> *BFOilBuiltins(void) {
    NSMutableArray *out = [NSMutableArray array];
    ListView lv;
    if (!OilList(lv)) return out;
    for (int i = 0; i < lv.size; i++) {
        void *d = lv.items[i];
        if (!d) continue;
        OilDescOffsets(d);
        NSString *nm = sOdName >= 0 ? Str(At<void *>(d, sOdName)) : nil;
        if (nm) {                                  // the game's names can carry rich-text tags: "<size=50>2011 USBC Masters</size>"
            static NSRegularExpression *tags;
            if (!tags) tags = [NSRegularExpression regularExpressionWithPattern:@"<[^>]*>" options:0 error:nil];
            nm = [[tags stringByReplacingMatchesInString:nm options:0 range:NSMakeRange(0, nm.length) withTemplate:@""]
                  stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
        }
        [out addObject:@{ @"index": @(i), @"name": nm.length ? nm : [NSString stringWithFormat:@"Pattern %d", i + 1],
                          @"feet": @(sOdDist >= 0 ? At<float>(d, sOdDist) : 0), @"ml": @(sOdVol >= 0 ? At<float>(d, sOdVol) : 0) }];
    }
    return out;
}

// fwd/rev nil: the game's own pattern at templateIndex (its steps, as the game reads them, and its drop).
// Otherwise: our copy of the engine draws the given steps.
NSDictionary *BFOilCompute(int templateIndex, NSArray *fwd, NSArray *rev, int drop, BOOL exact, int feet, BOOL precise) {
    @try {
        if (!fwd && !rev) {                        // one of the game's own patterns: its real Kegel file (microliters, exact ends)
            NSDictionary *spec = BFOilBuiltinSpec(templateIndex);
            if (spec) return KegelDrawPattern(spec[@"fwd"], spec[@"rev"], [spec[@"drop"] intValue], true, true, [spec[@"feet"] intValue], true);
            return KegelRun(templateIndex, nil, nil, 0);
        }
        return KegelDrawPattern(fwd, rev, drop, exact, true, feet, precise);
    } @catch (NSException *e) { return nil; }
}

NSArray<UIColor *> *BFOilColors(int n, float *maxHeight) {
    NSMutableArray *out = [NSMutableArray array];
    void *cd = ReadStaticObj(sOgColor, N.OilGen, "_colorData");
    static int gOff = -2, mOff = -2;
    if (cd && gOff == -2) { gOff = FieldOffset(ClassOf(cd), "OilGradient"); mOff = FieldOffset(ClassOf(cd), "MaxHeight"); }
    void *grad = (cd && gOff >= 0) ? At<void *>(cd, gOff) : nullptr;
    if (maxHeight) *maxHeight = (cd && mOff >= 0) ? At<float>(cd, mOff) : 0;
    const MethodInfo *eval = grad ? FindMethod(ClassOf(grad), "Evaluate", 1) : nullptr;
    for (int i = 0; i < n && eval; i++) {
        float t = n > 1 ? (float)i / (n - 1) : 0;
        void *a[] = { &t };
        bool ok = false;
        Il2CppObject *c = Invoke(eval, grad, a, &ok);
        if (!ok || !c) break;
        float *f = (float *)Unbox(c);
        [out addObject:[UIColor colorWithRed:f[0] green:f[1] blue:f[2] alpha:f[3]]];
    }
    return out;
}

void BFOilSetCustom(NSDictionary *pattern) { sCustomPattern = [pattern copy]; }

bool BFPracticeLobbyOpen(void) {
    if (!N.ok || !sSettled || !N.tFloatWnd) return false;
    static Ref lobby;
    static CFAbsoluteTime lastSearch = 0;
    void *w = lobby.get();
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    if (!w && now - lastSearch > 0.45) {          // called from the UI timer, so time-based, not frame-based
        lastSearch = now;
        Il2CppArray *all = FindAll(N.tFloatWnd);
        for (size_t i = 0; i < Len(all); i++) {
            void *x = Elem(all, i);
            if (Alive(x) && strcmp(ClassName(ClassOf(x)), "PracticeOil") == 0) { w = x; break; }
        }
        lobby.set(w);
    }
    return w && GOActive(GameObjectOf(w));
}

NSString *BFOilStatusLine(void) {
    NSMutableArray *parts = [NSMutableArray array];
    if (gBF.oilMirrorFix && sMirrorLanes) [parts addObject:@"oil drawn on the correct side"];
    if (gBF.oilInvisible) [parts addObject:[NSString stringWithFormat:@"invisible oil %@", sInvisNote.length ? [NSString stringWithFormat:@"(%@)", sInvisNote] : @""]];
    if (sCustomPattern) [parts addObject:sCustomAppliedId && !sCustomNote.length ? [NSString stringWithFormat:@"custom \"%@\" on the lane", sCustomPattern[@"name"]]
                                         : [NSString stringWithFormat:@"custom \"%@\" %@", sCustomPattern[@"name"], sCustomNote.length ? sCustomNote : @"(starts in practice)"]];
    return parts.count ? [parts componentsJoinedByString:@" \u00B7 "] : @"";
}

// ---- tap the game's pin layouts to pick pins (Practice) ----
// Holding the ball, the top-right pin layout (MainMenuButtonMan.pinObj) shows which pins stand; in the overhead
// view of the ball return, the little screen under it (the "MonitorCollider" box) does. A tap on either opens
// the pin picker for this shot only.
static Ref sMmbm, sMonCol;
static NSString *sTapNote = @"no tap yet";

static uint16_t StandingMask() {
    void *rpt = sRPT.get();
    if (!rpt || N.rpt_kegsUp < 0) return BF_ALL_PINS;
    Il2CppArray *kegs = At<Il2CppArray *>(rpt, N.rpt_kegsUp);
    size_t n = Len(kegs);
    if (!n || n > 32) return BF_ALL_PINS;
    bool *k = (bool *)Data(kegs);
    uint16_t m = 0;
    for (size_t i = 0; i < 10 && i < n; i++) if (k[i]) m |= (uint16_t)(1u << i);
    return m ? m : BF_ALL_PINS;
}

static bool ScreenWH(float &w, float &h) {
    if (!N.Scr_w || !N.Scr_h) return false;
    w = (float)InvokeInt(N.Scr_w, nullptr, nullptr, 0);
    h = (float)InvokeInt(N.Scr_h, nullptr, nullptr, 0);
    return w > 1 && h > 1;
}

static bool ProjectPoint(void *cam, Vec3 p, float &x, float &y) {      // world -> screen pixels (y up); false when behind the camera
    if (!Alive(cam) || !N.Cam_w2s) return false;
    void *a[] = { &p };
    bool ok = false;
    Il2CppObject *b = Invoke(N.Cam_w2s, cam, a, &ok);
    if (!ok || !b) return false;
    Vec3 q = *(Vec3 *)Unbox(b);
    if (q.z <= 0) return false;
    x = q.x; y = q.y;
    return true;
}

static void *WorldCameraFor(int layer) {           // the enabled camera that draws this layer to the screen, highest depth
    Il2CppArray *cams = N.Cam_all ? (Il2CppArray *)Invoke(N.Cam_all, nullptr, nullptr) : nullptr;
    void *best = nullptr;
    float bestDepth = -1e9f;
    for (size_t i = 0; i < Len(cams); i++) {
        void *c = Elem(cams, i);
        if (!Alive(c)) continue;
        if (N.Beh_enabled && !InvokeBool(N.Beh_enabled, c, nullptr, true)) continue;
        if (N.Cam_target && Invoke(N.Cam_target, c, nullptr)) continue;                    // draws into a texture, not the screen
        int mask = N.Cam_mask ? InvokeInt(N.Cam_mask, c, nullptr, -1) : -1;
        if (layer >= 0 && layer < 32 && !((mask >> layer) & 1)) continue;
        Il2CppObject *db = N.Cam_depth ? Invoke(N.Cam_depth, c, nullptr) : nullptr;
        float d = db ? *(float *)Unbox(db) : 0;
        if (d > bestDepth) { bestDepth = d; best = c; }
    }
    if (!best && N.Cam_main) { void *m = Invoke(N.Cam_main, nullptr, nullptr); if (Alive(m)) best = m; }
    return best;
}

// a UI object's rectangle on screen, as 0..1 of the screen with y up: {minU, minV, maxU, maxV}
static bool UiRectOnScreen(void *go, float r[4]) {
    if (!Alive(go) || !N.GO_getTransform || !N.RT_corners || !N.Vec3Cls || !N.GO_activeH) return false;
    if (!InvokeBool(N.GO_activeH, go, nullptr, false)) return false;
    void *tr = Invoke(N.GO_getTransform, go, nullptr);
    float sw, sh;
    if (!Alive(tr) || !ScreenWH(sw, sh)) return false;
    Il2CppArray *arr = NewArray(N.Vec3Cls, 4);
    if (!arr) return false;
    void *a[] = { arr };
    Invoke(N.RT_corners, tr, a);
    Vec3 *c = (Vec3 *)Data(arr);
    int mode = 0;
    void *cam = nullptr;
    void *cv = nullptr;
    if (N.GO_inParent && N.tCanvas) { void *ca[] = { N.tCanvas }; cv = Invoke(N.GO_inParent, go, ca); }
    if (Alive(cv)) {
        void *root = N.Cv_root ? Invoke(N.Cv_root, cv, nullptr) : cv;
        if (Alive(root)) cv = root;
        mode = N.Cv_mode ? InvokeInt(N.Cv_mode, cv, nullptr, 0) : 0;
        cam = (mode != 0 && N.Cv_cam) ? Invoke(N.Cv_cam, cv, nullptr) : nullptr;
    }
    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
    for (int i = 0; i < 4; i++) {
        float x = c[i].x, y = c[i].y;
        if (mode != 0 && Alive(cam) && !ProjectPoint(cam, c[i], x, y)) return false;      // overlay canvas: already pixels
        mnx = fminf(mnx, x); mxx = fmaxf(mxx, x); mny = fminf(mny, y); mxy = fmaxf(mxy, y);
    }
    r[0] = mnx / sw; r[1] = mny / sh; r[2] = mxx / sw; r[3] = mxy / sh;
    return r[2] > r[0] && r[3] > r[1];
}

// a collider's box on screen (the little screen under the ball return)
static bool ColliderRectOnScreen(void *col, float r[4]) {
    if (!Alive(col) || !N.Col_bounds) return false;
    void *go = GameObjectOf(col);
    float sw, sh;
    if (!Alive(go) || !ScreenWH(sw, sh)) return false;
    if (N.GO_activeH && !InvokeBool(N.GO_activeH, go, nullptr, false)) return false;
    bool ok = false;
    Il2CppObject *b = Invoke(N.Col_bounds, col, nullptr, &ok);
    if (!ok || !b) return false;
    const float *f = (const float *)Unbox(b);                       // Bounds: center xyz, extents xyz
    int layer = N.GO_layer ? InvokeInt(N.GO_layer, go, nullptr, -1) : -1;
    void *cam = WorldCameraFor(layer);
    if (!Alive(cam)) return false;
    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
    for (int i = 0; i < 8; i++) {
        Vec3 p = { f[0] + ((i & 1) ? f[3] : -f[3]), f[1] + ((i & 2) ? f[4] : -f[4]), f[2] + ((i & 4) ? f[5] : -f[5]) };
        float x, y;
        if (!ProjectPoint(cam, p, x, y)) return false;
        mnx = fminf(mnx, x); mxx = fmaxf(mxx, x); mny = fminf(mny, y); mxy = fmaxf(mxy, y);
    }
    r[0] = mnx / sw; r[1] = mny / sh; r[2] = mxx / sw; r[3] = mxy / sh;
    return r[2] > r[0] && r[3] > r[1];
}

static bool Inside(const float r[4], float u, float v) {            // with a little extra room for a fingertip
    float mx = (r[2] - r[0]) * 0.12f + 0.012f, my = (r[3] - r[1]) * 0.12f + 0.012f;
    return u >= r[0] - mx && u <= r[2] + mx && v >= r[1] - my && v <= r[3] + my;
}

// u, v: the tap as 0..1 of the screen, v up. Returns true when it opened the pin picker.
bool BFPinTapAt(float u, float v) {
    if (!N.ok || !sSettled || gBFSafeMode || !gBFStatus.offline || sMode != MODE_FUN || sInTutorial) return false;
    if (BFMenuPickerVisible() || BFMenuVisible()) return false;
    NSString *what = nil;
    float r[4] = {};
    if (sLoc == LOC_START_POS) {                                     // holding the ball: the top-right layout
        void *m = sMmbm.get();
        if (!m && N.tMMBM) { m = FirstAlive(FindAll(N.tMMBM)); sMmbm.set(m); }
        if (m && N.mmbm_pinBack >= 0 && UiRectOnScreen(At<void *>(m, N.mmbm_pinBack), r) && Inside(r, u, v)) what = @"top-right pin layout";
        else if (m && N.mmbm_pinObj >= 0 && UiRectOnScreen(At<void *>(m, N.mmbm_pinObj), r) && Inside(r, u, v)) what = @"top-right pin layout";
    } else if (sLoc == LOC_BALL_RETURNER || sLoc == LOC_BOTTOM_MONITOR) {   // overhead of the ball return: the screen under it
        void *c = sMonCol.get();
        if (!c && N.tBoxCollider) {
            Il2CppArray *all = FindAll(N.tBoxCollider);
            for (size_t i = 0; i < Len(all) && !c; i++) {
                void *o = Elem(all, i);
                if (Alive(o) && [NameOf(o) isEqualToString:@"MonitorCollider"]) c = o;
            }
            sMonCol.set(c);
        }
        if (c && ColliderRectOnScreen(c, r) && Inside(r, u, v)) what = @"screen under the ball return";
    }
    sTapNote = [NSString stringWithFormat:@"loc=%d tap u=%.3f v=%.3f rect=%.2f,%.2f-%.2f,%.2f -> %@", sLoc, u, v, r[0], r[1], r[2], r[3], what ?: @"not on a pin layout"];
    if (!what) return false;
    BFLog(@"pin layout tapped (%@): opening the pin picker for this shot", what);
    BFMenuShowPinPickerOneShot(StandingMask());
    return true;
}

// ---------------------------------------------------------------------------
// Main loop + menu text
// ---------------------------------------------------------------------------
void BFEngineTick(void) {
    sFrame++;
    if (sStartTime == 0) sStartTime = CFAbsoluteTimeGetCurrent();
    if (gBFSafeMode) return;
    if (!N.ok) {
        static int attempts = 0;
        // hands off for the first 5 seconds while the game boots
        if (sFrame < 300 || sFrame % 30 != 0 || attempts > 200) return;
        if (!Ready()) return;
        attempts++;
        try { N.ok = Resolve(); } catch (...) { N.ok = false; BFLog(@"error while connecting to the game"); }
        gBFStatus.engineReady = N.ok;
        if (!N.ok) return;
    }
    @try {
        try {
            UpdateState();
            TutorialTick();
            StuckTick();
            SpinnerTick();
            DiagTick();
            OilTick();
            FpsTick();
            PinFixTick();
            PinImageTick();
            BallCCDTick();
            SpeedTick();
            SpinTick();
            SpareTick();
            CurrentBallTick();
            SkinDataTick();
            SkinRefreshTick();
            InHandFallbackTick();
            ArsenalTick();
        } catch (...) {
            BFLog(@"C++ exception in tick");
        }
    } @catch (NSException *e) {
        BFLog(@"ObjC exception in tick: %@", e);
    }
    sPrevLoc = sLoc;
}

NSString *BFStatusLine(void) {
    if (gBFSafeMode) return @"Safe mode: the game didn't finish starting twice in a row, so BowlingPlus paused itself to keep the game working.";
    if (!gBFStatus.engineReady || !sSettled) return @"Waiting for the game to finish loading...";
    if (sInTutorial) return @"Tutorial: use the Skip tutorial button at the top right";
    if (gBFStatus.offline) return @"Practice (offline): everything is active";
    if (gBFStatus.gameMode == MODE_COMPETE) return @"Online match: fun stuff is paused";
    if (gBFStatus.gameMode == MODE_TUTORIAL) return @"Tutorial: fun stuff is paused";
    return @"Menus / online: fun stuff is paused";
}

NSString *BFBallLine(void) {
    NSMutableArray<NSString *> *lines = [NSMutableArray array];
    if (sBallLine.length) [lines addObject:sBallLine];
    if (!gBF.textureFix) {
        [lines addObject:@"Skin fix: off"];
        return [lines componentsJoinedByString:@"\n"];
    }
    switch (sPearlFail) {
        case PF_OK:      [lines addObject:@"Match Up Pearl: fixed (its burgundy art, from the game's own Pearl/Hybrid skin file)"]; break;
        case PF_WAITING: [lines addObject:@"Match Up Pearl: waiting for the store data..."]; break;
        case PF_NO_PEARL:[lines addObject:@"Match Up Pearl: not in the store list"]; break;
        case PF_NO_SHEET:[lines addObject:@"Match Up Pearl: its skin file isn't in the game's catalog, fixed in your hand only"]; break;
        default:         [lines addObject:@"Match Up Pearl: couldn't change the game data, fixed in your hand only"]; break;
    }
    if (sBPFixed) [lines addObject:@"Match Up BP: fixed (its Black Pearl art; the game pointed it at a file that isn't in the app)"];
    else if (sCurSkin == SKIN_BP && !sBPHasSkin && sPearlFail != PF_WAITING)
        [lines addObject:@"Match Up BP: no working skin found, drawn look-alike in your hand"];
    return [lines componentsJoinedByString:@"\n"];
}

NSString *BFArsenalLine(void) {
    if (!sQuery) return @"Type part of a ball name and tap Search, then open the Arsenal.";
    if (sShown >= 0) return [NSString stringWithFormat:@"Showing %d of %d balls for \"%@\"", sShown, sTotal, sQuery];
    return [NSString stringWithFormat:@"Searching for \"%@\" - open the Arsenal", sQuery];
}

static float InvokeFloat(const MethodInfo *m, void *obj, float fallback) {
    bool ok = false;
    Il2CppObject *r = Invoke(m, obj, nullptr, &ok);
    return (ok && r) ? *(float *)Unbox(r) : fallback;
}

static NSString *PathInfo(int id) {          // "123 Text_X_Y.png" for the debug info
    if (id < 0) return @"none";
    NSString *p = DlcPath(id);
    if (!p.length) return [NSString stringWithFormat:@"%d (not in catalog)", id];
    NSString *file = [p lastPathComponent];
    return [NSString stringWithFormat:@"%d %@%@", id, file, Shipped(id) == 0 ? @" (NOT in app files)" : @""];
}

NSString *BFDebugInfo(void) {
    struct utsname u;
    uname(&u);
    NSMutableString *s = [NSMutableString string];
    [s appendFormat:@"BowlingPlus v%@ | iOS %@ | %s\n", BF_VERSION, [UIDevice currentDevice].systemVersion, u.machine];
    [s appendFormat:@"engine=%d settled=%d safe=%d mode=%d loc=%d offline=%d tutorial=%d frame=%d\n",
        gBFStatus.engineReady, sSettled, gBFSafeMode, sMode, sLoc, gBFStatus.offline, sInTutorial, sFrame];
    [s appendFormat:@"cfg: skin=%d pins=%d pinSpec=%d speed=%.1f spare=%d auto=%d mask=0x%03x fps120=%d\n",
        gBF.textureFix, gBF.pinFix, gBF.pinSpec, gBF.speedMult, gBF.spareMode, gBF.spareAuto, gBF.lastPinMask, gBF.fps120];
    [s appendFormat:@"%@ | skinKind=%d\n", sBallLine.length ? sBallLine : @"no current ball", sCurSkin];
    bool live = N.ok && !gBFSafeMode && sSettled;
    ListView cat;
    void *vd = live ? VisualData() : nullptr;
    [s appendFormat:@"catalog: %d entries, %lu app assets\n", (vd && ReadList(At<void *>(vd, N.ivd_items), cat)) ? cat.size : -1,
        (unsigned long)AppAssetNames().count];
    [s appendFormat:@"pearl fix: fail=%d fixed=%d sheet=%d | game had: %@ texc=%d | bp fixed=%d hasSkin=%d (game had %@)\n",
        sPearlFail, sPearlFixed, sPearlSheet, live ? PathInfo(sGamePearlDlc) : @"-", sGamePearlTexc, sBPFixed, sBPHasSkin,
        live && sBPLink.changed ? PathInfo(sBPLink.origDlc) : @"-"];
    struct { const char *name; ObjRef *ref; } balls[] = { { "Pearl", &sPearlShop }, { "Hybrid", &sHybridShop }, { "BP", &sBPShop }, { "Solid", &sSolidShop } };
    for (auto &b : balls) {
        void *it = b.ref->get();
        if (!it) { [s appendFormat:@"%s: not found\n", b.name]; continue; }
        [s appendFormat:@"%s: skin %@ texc=%d\n", b.name, live ? PathInfo(SkinDlcOf(it)) : @"-", TexcOf(it)];
    }
    [s appendFormat:@"arsenal: query=%@ shown=%d total=%d\n", sQuery ?: @"-", sShown, sTotal];
    int tgt = live && N.App_getFps ? InvokeInt(N.App_getFps, nullptr, nullptr, -1) : -1;
    [s appendFormat:@"fps: on=%d applied=%d target=%d table(orig)=%d/%d plist120=%d screenMax=%ld | %@\n",
        gBF.fps120, sFpsApplied, tgt, sOrigMenuFps, sOrigGameFps, PlistAllows120(), (long)[UIScreen mainScreen].maximumFramesPerSecond,
        BFPrivacyDebug()];
    {
        ListView ol;
        int nOils = OilList(ol) ? ol.size : -1;
        [s appendFormat:@"oil textures ready=%d | thickness colors=%@ | ", sPrewarmDone, sThickApplied ? @"on" : @"off"];
        [s appendFormat:@"oil: mirror=%d lanes=%d breakdown=%d redraws=%d invisible=%d picks=%d custom=%@ applied=%@ sel=%d patterns=%d %@\n",
            gBF.oilMirrorFix, sMirrorLanes, gBF.oilBreakdown, sBreakdownRedraws, gBF.oilInvisible, sInvisPicks,
            sCustomPattern[@"name"] ?: @"-", sCustomAppliedId ? @"yes" : @"no", live ? OilSelected() : -1, nOils, sCustomNote];
        [s appendString:OilReport(live)];
    }
    {
        [s appendFormat:@"pin image: on=%d materials=%d size=%d fails=%d\n", gBF.pinImage, sPinImgMats, sPinImgSize, sPinImgFails];
    }
    [s appendFormat:@"loading: unstick=%d rescued=%d spinner=%d | ipv4=%d dnsSlots=%d | pinSpec(pins only)=%d ball=%d\n",
        gBF.unstick, sUnstuckCount, sSpinnerCount, gBF.gameIPv4, BFDnsHookSlots(), gBF.pinSpec, BallCDM()];
    {
        void *inv = SpinInventary();
        [s appendFormat:@"pin tap: %@\n", sTapNote];
    [s appendFormat:@"spin boost: x%.1f last %d -> %d rpm grip x%.2f | rpmFactor=%.3f maxOmega=%.1f\n", gBF.spinMult, sSpunFrom, sSpunTo, sGrip,
            (inv && N.inv_rpmFactor >= 0) ? At<float>(inv, N.inv_rpmFactor) : -1.f, (inv && N.inv_maxOmega >= 0) ? At<float>(inv, N.inv_maxOmega) : -1.f];
    }
    void *holder = sHolders[0].get();
    ListView lv;
    if (live && holder && N.ph_pins >= 0 && ReadList(At<void *>(holder, N.ph_pins), lv) && lv.size > 0 && lv.items[0]) {
        void *rb = GetComp(GameObjectOf(At<void *>(lv.items[0], N.pin_physic)), N.tRigidbody);
        if (rb) [s appendFormat:@"pin1: cdm=%d kinematic=%d linDamp=%.3f angDamp=%.3f pins=%d\n",
                    InvokeInt(N.RB_getCDM, rb, nullptr, -1), InvokeBool(N.RB_isKinematic, rb, nullptr, false),
                    N.RB_getLinDamp ? InvokeFloat(N.RB_getLinDamp, rb, -1) : -1.f,
                    N.RB_getAngDamp ? InvokeFloat(N.RB_getAngDamp, rb, -1) : -1.f, lv.size];
    }
    return s;
}
