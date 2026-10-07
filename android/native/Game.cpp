// Port of src/Game.mm for Android. The game logic is the iOS code unchanged; only what used Foundation /
// UIKit (strings, dictionaries, files, the screen) is replaced. Runs on Unity's main thread (Jni.cpp).
#include "BFShared.h"
#include "Il2Cpp.h"
#include <math.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include "KegelParse.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>

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
    const MethodInfo *R_getSharedMat, *M_hasProp, *M_getFloatI, *M_setFloatS, *M_getTexI, *Sh_propToId, *T_getWrap, *T_setWrap, *T_getH;
    int og_lines, ri_oilTex;
    const MethodInfo *Proc_Hide;
    int proc_count;
    Il2CppObject *tFloatWnd;
    const MethodInfo *App_reach;
    const MethodInfo *App_getFps, *App_setFps, *App_streaming;
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
    const MethodInfo *Comp_getTransform, *GO_getTransform, *Tr_getRot, *Tr_setRot, *Tr_getLocalRot, *Tr_setLocalRot,
                     *Tr_getLocalPos, *Tr_setLocalPos, *Tr_setLocalScale;
    int ph_pins, pin_physic, ldt_dlcID, ldt_toChange, ldt_objectID, item_itemId, item_baseId, shop_name, si_dlcLink;
    int ih_shop, ars_ballsData, ars_scroll;
    FieldInfo *gp_gameMode, *gp_location, *inv_currentBall, *ih_instance, *invd_instance;
} N;

static int Off(Il2CppClass *k, const char *name) {
    int o = k ? FieldOffset(k, name) : -1;
    if (k && o < 0) BFLog("field not found: %s", name);
    return o;
}

static const MethodInfo *Meth(Il2CppClass *k, const char *name, int argc, const char *p0 = nullptr, const char *p1 = nullptr) {
    const MethodInfo *m = k ? FindMethod(k, name, argc, p0, p1) : nullptr;
    if (k && !m) BFLog("method not found: %s", name);
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
    N.App_streaming = Meth(N.Application, "get_streamingAssetsPath", 0);   // Android: jar:file://<apk>!/assets
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
    N.T_getH           = Meth(texture, "get_height", 0);
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
        N.Tr_getLocalPos = Meth(tr, "get_localPosition", 0);
        N.Tr_setLocalPos = Meth(tr, "set_localPosition", 1);
        N.Tr_setLocalScale = Meth(tr, "set_localScale", 1);
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
    BFLog("connected to the game");
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

static Str NameOf(void *unityObject) {
    return (Alive(unityObject) && N.GetName) ? Text(Invoke(N.GetName, unityObject, nullptr)) : Str();
}

static void DontUnload(void *unityObject) {   // HideFlags.DontUnloadUnusedAsset
    if (!N.SetHideFlags || !Alive(unityObject)) return;
    int flags = 32;
    void *a[] = { &flags };
    Invoke(N.SetHideFlags, unityObject, a);
}

static Str Squash(const Str &s) {             // "Match-Up BP" -> "matchupbp"
    Str o;
    for (unsigned char c : s) if (c < 128 && isalnum((int)c)) o += (char)tolower((int)c);
    return o;
}

static const std::vector<Json> &Items(const Json *j) { return j ? j->items() : Json::Nil().items(); }

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
static double sRPTSince = 0;    // when the lane controller showed up
static bool sSettled = false;           // lane up for a few seconds: safe to use game data
static bool sHealthy = false;

static void NotReady() {
    sSettled = false;
    sMode = sLoc = -1;
    gBFStatus.offline = false;
    gBFStatus.gameMode = gBFStatus.location = -1;
}

static Ref sCoreLoop;                   // MonoCoreLoop: the game's startup / menu state machine

// The game's own main menu is up, so its startup (scene, data, login) is done. Exact name only: any other
// state keeps the plain 4 s wait. (The game has many "...State" classes; this is the one seen on a device.)
static bool AtMainMenu() {
    if (!N.tMonoCoreLoop || N.mcl_sm < 0 || N.sm_state < 0) return false;
    void *mcl = sCoreLoop.get();
    if (!mcl && sFrame % 30 == 0) { mcl = FirstAlive(FindAll(N.tMonoCoreLoop)); sCoreLoop.set(mcl); }
    void *sm = mcl ? At<void *>(mcl, N.mcl_sm) : nullptr;
    void *state = sm ? At<void *>(sm, N.sm_state) : nullptr;
    const char *name = state ? ClassName(ClassOf(state)) : nullptr;
    return name && strcmp(name, "MainMenuState") == 0;
}

static void UpdateState() {
    void *rpt = sRPT.get();
    if (!rpt && sFrame % 30 == 0) {
        rpt = FirstAlive(FindAll(N.tRunPsycsTest));
        sRPT.set(rpt);
        if (rpt) { sRPTSince = BFNow(); BFLog("lane found, waiting for it to settle"); }
    }
    if (!rpt) { NotReady(); return; }
    double up = BFNow() - sRPTSince;
    // Let the game finish loading before touching anything. The game's main menu being up IS "loading
    // finished" (scene, data and login are done; it's also the first screen Practice can be reached from),
    // so BowlingPlus starts right then. Before 1.6.2 it always waited 4 s after the lane appeared, and 1.6.2
    // waited 1.5 s more after the main menu: both let a quick tap into Practice beat the oil, colors and
    // ball fixes. If the main menu can't be seen, the plain 4 s wait still applies.
    if (!sSettled && !(up >= 4.0 || AtMainMenu())) { NotReady(); return; }
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
    if (!sSettled) BFLog("settled %.1f s after the lane appeared%s", up, up < 4.0 ? " (main menu up)" : "");
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

static void *BallBody() {
    void *rpt = sRPT.get();
    if (!rpt || N.rpt_sphere < 0) return nullptr;
    return GetComp(At<void *>(rpt, N.rpt_sphere), N.tRigidbody);
}


#define PIN_TURN_CLOCK BFNow()
#define PIN_TURN_LOG "pin turns: %s"
// ---- pins keep their turn (Practice) ----
// What the game does (read from its code, 1.907, the same on iOS and Android):
//  - The pins on the lane are InventaryData.kegels (UsePinHolder is off; PinHolder's pins never move).
//  - RunPsycsTest.UpdatePinPositions racks them. It first copies _kegsUp into the static
//    mdl_ShootCurrentData.CurrentData.Before, then for each kegel i, in order: moves it to its spot (or 200 m
//    under the floor when _kegsUp[i] is false), sets transform.rotation = Euler(0, 0, Random.Range(0, 16) x 22.5)
//    (a random turn about the vertical axis, world Z), resets and sleeps its Rigidbody, and puts its visible
//    model on the spot.
//  - It runs on every rack: after each throw (Reset, from endThrought) and on every ball pickup or switch
//    (ChangeBall -> RecalculatePinsUpState -> UpdatePinPositions), with the same pins up. So each pickup gave
//    every pin a new random turn.
// The fix (1.6.6): Random.Range(int, int) doesn't call the engine directly. It loads the engine's function from a
// pointer IL2CPP fills on first use (in the game's writable data), puts the caller's return address back and
// jumps to it. BowlingPlus points that pointer at its own function (BFRandomRangeInt), which always draws the
// game's own random number and, only for the call from UpdatePinPositions' Random.Range(0, 16), may hand back the
// pin's kept turn instead. That happens inside the rack, before the game sets the rotation, so a kept turn is
// never drawn any other way, not even for one frame. (1.6.2-1.6.5 put turns back after the game had already
// turned the pins, so the new turn could always show for at least the frame it was racked in.) No game code is
// changed: it's a data pointer, which is all a non-jailbroken iPhone allows.
// The rule: a pin keeps its turn while it stands. It gets a new random turn when it comes back after being down:
// knocked over (when the rack starts, before its rotation is set, its body leans more than 15 degrees) or off the
// lane at the last rack. That new turn is decided in advance (sTurnPend, BowlingPlus's own random draw; the game's
// draw still happens and is discarded), so the pinsetter can already carry the pin with it (see the pinsetter part
// below). Outside Practice the game's own turn is used (and remembered, so nothing jumps when you come back).
static double PinNow() { return PIN_TURN_CLOCK; }   // seconds (platform clock for the shared pin code)

struct Quat { float x, y, z, w; };
static Ref sInvData;                       // InventaryData (also used by the pin image code and Copy debug info)

typedef int32_t (*BFRandIntFn)(int32_t, int32_t);
static BFRandIntFn *sRandSlot = nullptr;   // the game's pointer to the engine's Random.RandomRangeInt
static BFRandIntFn sRandEngine = nullptr;  // the engine function it pointed to
static uintptr_t sTurnSite[4];             // where UpdatePinPositions' Random.Range(0, 16) returns to
static int sTurnSites = 0;
static int sTurnHook = 0;                  // 0 not tried yet, 1 in place, -1 not possible (sTurnWhy)
static const char *sTurnWhy = "not tried yet";
static const int kTurnMax = 32;
static int32_t sTurnValue[kTurnMax];       // each kegel's turn, in 22.5 degree steps (the game's own draw)
static bool sTurnHave[kTurnMax], sTurnWasUp[kTurnMax], sTurnUpKnown[kTurnMax];
static int32_t sTurnPend[kTurnMax];        // the turn kegel i gets the next time it comes back up (decided in advance)
static bool sTurnPendHave[kTurnMax];
static uint32_t sTurnRng = 0;
static int sTurnNext = 0;                  // the kegel the next call is for (they come in order, one per kegel)
static int sTurnCount = 10;                // kegels per rack (InventaryData.kegels length)
static double sTurnLast = -1;
static bool sTurnKeep = false;             // Practice: keep turns
static FieldInfo *sShotData = nullptr;     // mdl_ShootCurrentData.CurrentData (looked up on first use, see TurnPinUp)
static int sShotBefore = -2;               // mdl_ShootData.Before (bool[]): the pins up for this rack
static int sKegelsOff = -2;                // InventaryData.kegels (GameObject[])
// Copy debug info
static int sTurnRacks = 0, sTurnKept = 0, sTurnNew = 0, sTurnKnocked = 0, sTurnOffLane = 0, sTurnOther = 0;
static int sTurnUpUnknown = 0, sTurnTiltFails = 0;
static float sTurnMaxStandTilt = 0;        // the most a kept (standing) pin leaned when racked

// The kegels array (and its length) from the static InventaryData._instance: no object pointer kept around.
static Il2CppArray *TurnKegels() {
    void *inv = ReadStaticObj(N.invd_instance, N.InventaryData, "_instance");
    if (!inv) return nullptr;
    if (sKegelsOff == -2) {
        char tn[64];
        sKegelsOff = (FieldTypeName(N.InventaryData, "kegels", tn, sizeof(tn)) && !strcmp(tn, "UnityEngine.GameObject[]")) ? FieldOffset(N.InventaryData, "kegels") : -1;
    }
    return sKegelsOff >= 0x10 ? At<Il2CppArray *>(inv, sKegelsOff) : nullptr;
}

// Is kegel i up for this rack? 1 up, 0 down, -1 unknown. Read from the copy UpdatePinPositions has just made
// (so the game has run that class's start-up code by now: looking it up here has no side effects).
static int TurnPinUp(int i) {
    if (sShotBefore == -2) {
        sShotBefore = -1;
        Il2CppClass *cur = FindClass("", "mdl_ShootCurrentData");
        Il2CppClass *data = FindClass("", "mdl_ShootData");
        char tn[64];
        if (cur && data && FieldTypeName(data, "Before", tn, sizeof(tn)) && !strcmp(tn, "System.Boolean[]")) {
            sShotData = StaticField(cur, "CurrentData");
            if (sShotData) sShotBefore = FieldOffset(data, "Before");
        }
    }
    if (!sShotData || sShotBefore < 0x10) return -1;
    void *shot = nullptr;
    StaticRead(sShotData, &shot);
    Il2CppArray *a = shot ? At<Il2CppArray *>(shot, sShotBefore) : nullptr;
    if (!a || (size_t)i >= Len(a)) return -1;
    return ((bool *)Data(a))[i] ? 1 : 0;
}

// How far kegel i leans, in degrees (-1 if it can't be read). Called before the game sets its rotation, so this
// is how it was left by the last throw. Standing, its axis (local Z) is world up: cos(lean) = 1 - 2(x^2 + y^2).
static float TurnPinLean(int i) {
    Il2CppArray *k = TurnKegels();
    void *go = (k && (size_t)i < Len(k)) ? Elem(k, i) : nullptr;
    if (!go || !N.GO_getTransform || !N.Tr_getRot) return -1;
    bool ok = false;
    void *tr = Invoke(N.GO_getTransform, go, nullptr, &ok);
    if (!ok || !tr) return -1;
    Il2CppObject *b = Invoke(N.Tr_getRot, tr, nullptr, &ok);
    if (!ok || !b) return -1;
    Quat q = *(Quat *)Unbox(b);
    float c = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    return acosf(fmaxf(-1.0f, fminf(1.0f, c))) * 57.29578f;
}

static int32_t TurnRandom16() {                    // xorshift32, seeded from the clock
    if (!sTurnRng) sTurnRng = (uint32_t)fmod(PinNow() * 1000.0, 4294967291.0) | 1u;
    sTurnRng ^= sTurnRng << 13;
    sTurnRng ^= sTurnRng >> 17;
    sTurnRng ^= sTurnRng << 5;
    return (int32_t)(sTurnRng % 16u);
}

static int32_t TurnPending(int i) {
    if (!sTurnPendHave[i]) { sTurnPend[i] = TurnRandom16(); sTurnPendHave[i] = true; }
    return sTurnPend[i];
}

// The turn kegel i will get at the next rack if it's up then: the same rule as TurnPick, decided now.
static int32_t TurnPredict(int i) {
    if (sTurnHave[i] && (sTurnWasUp[i] || !sTurnUpKnown[i]) && TurnPinLean(i) <= 15.0f) return sTurnValue[i];
    return TurnPending(i);
}

// For the pin physics counts (PinPhysRack, further down): what this rack says about the throw before it.
static int sRackKnocked = 0;
static bool sRackFull = true;
static unsigned sRackLeave = 0;
static void PinPhysRack(int knocked, bool fullRack, unsigned leave);

// Kegel i's turn for this rack. r is the game's own random draw.
static int32_t TurnPick(int32_t r) {
    double now = PinNow();
    if (now - sTurnLast > 0.25 || now < sTurnLast) {   // a new rack (the 10 calls come within a millisecond)
        sTurnNext = 0;
        Il2CppArray *k = sTurnKeep ? TurnKegels() : nullptr;
        if (k && Len(k) >= 1 && Len(k) <= (size_t)kTurnMax) sTurnCount = (int)Len(k);
    }
    sTurnLast = now;
    int i = sTurnNext % sTurnCount;                    // (two racks in a row wrap around)
    sTurnNext = i + 1;
    if (i == 0) { sTurnRacks++; sRackKnocked = 0; sRackFull = true; sRackLeave = 0; }
    int up = TurnPinUp(i);
    if (up < 0) sTurnUpUnknown++;
    bool keep = false, wasUp = sTurnHave[i] && sTurnWasUp[i];
    if (!wasUp) sRackFull = false;
    if (sTurnKeep && sTurnHave[i]) {
        if (up == 0) {                                 // off the lane: its turn doesn't show, keep it as it is
            keep = true;
            sTurnOffLane++;
            if (wasUp) sRackKnocked++;                 // (the game put it down: it fell in the last throw)
        } else if (sTurnWasUp[i] || up < 0) {         // standing at the last rack: kept unless it was knocked over
            float lean = TurnPinLean(i);
            if (lean < 0) sTurnTiltFails++;
            if (lean > 15.0f) { sTurnKnocked++; if (wasUp) sRackKnocked++; }
            else {
                keep = true;
                sTurnKept++;
                if (lean > sTurnMaxStandTilt) sTurnMaxStandTilt = lean;
                if (wasUp) sRackLeave |= 1u << i;
            }
        }
    }
    if (keep) r = sTurnValue[i];
    else {
        if (sTurnKeep) {                               // the turn decided in advance, then the next one
            r = TurnPending(i);
            sTurnPend[i] = TurnRandom16();
            sTurnNew++;
        }
        sTurnValue[i] = r;
        sTurnHave[i] = true;
    }
    if (up >= 0) sTurnWasUp[i] = up == 1;
    sTurnUpKnown[i] = up >= 0;
    if (i == sTurnCount - 1 && sTurnKeep) PinPhysRack(sRackKnocked, sRackFull, sRackLeave);
    return r;
}

// ---- the pinsetter's pins show the same turns (Practice) ----
// What the game does (1.907, read from its code and scene): when the pinsetter lifts the standing pins for your
// second ball, PinSetterManager.Update hides the real pins' visible models (kegels_renderers) and shows its own
// pin models instead, InventaryData.pinseterKegelRenderer[pin number - 1], which hang from the animated pinsetter
// joints with one fixed orientation; when it sets a new rack it shows them on the way down, and the real pins are
// racked (UpdatePinPositions) when it's done. Every frame it also copies their world position and rotation onto
// pindeckKegelRenderer. PinSetterManager.mirrorOnPinsetter[i] are their reflections, under a second, upside-down
// copy of the machine. So every lifted or lowered pin showed the same face, whatever its turn.
// The fix: each model's root (static local rotation; only the joints are animated) gets an extra twist about its
// own long axis (root local Z) so it shows the turn its pin has, or will get at the next rack (TurnPredict). From
// the clips, at the pick-up and the placing pose every model root stands upright turned -3.544 degrees, so the
// twist is turn + 3.544; the upside-down reflections need 3.544 - turn. A model is only changed while it's hidden.
static const float kSetterTwist = 3.544f;
static Ref sSetterMgr;
static int sSetterPinsOff = -2, sSetterMirrorOff = -2;
static void *sSetterGo[kTurnMax], *sMirrorGo[kTurnMax];     // identity only (to notice a new scene), never read
static Quat sSetterBase[kTurnMax], sMirrorBase[kTurnMax];    // each model root's own local rotation
static int sSetterShown[kTurnMax], sMirrorShown[kTurnMax];   // the turn it shows (-1: the game's own, -2: not set yet)
static int sSetterSets = 0, sSetterFails = 0, sSetterBusy = 0;
static double sSetterLast = -1;

static Quat QMul(Quat a, Quat b) {
    Quat r;
    r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    return r;
}

static Quat QTurnZ(float deg) {
    float h = deg * 0.008726646f;                      // half the angle, in radians
    Quat q = { 0.0f, 0.0f, sinf(h), cosf(h) };
    return q;
}

// Sets one model root to its own rotation turned `deg` about its long axis (or back to its own rotation).
// Returns 1 done, 0 skipped (shown right now), -1 failed.
static int SetterTwist(void *go, void **seen, Quat *base, float deg, bool restore) {
    if (!go) return -1;
    if (N.GO_activeH && InvokeBool(N.GO_activeH, go, nullptr, false)) return 0;
    bool ok = false;
    void *tr = Invoke(N.GO_getTransform, go, nullptr, &ok);
    if (!ok || !tr) return -1;
    if (*seen != go) {                                 // first time (or a new scene): remember its own rotation
        Il2CppObject *b = Invoke(N.Tr_getLocalRot, tr, nullptr, &ok);
        if (!ok || !b) return -1;
        *base = *(Quat *)Unbox(b);
        *seen = go;
    }
    Quat q = restore ? *base : QMul(*base, QTurnZ(deg));
    void *a[] = { &q };
    Invoke(N.Tr_setLocalRot, tr, a, &ok);
    return ok ? 1 : -1;
}

static void PinsetterTurnTick() {
    if (!N.GO_getTransform || !N.Tr_getLocalRot || !N.Tr_setLocalRot || sTurnHook != 1) return;
    double now = PinNow();
    if (now - sSetterLast < 0.1 && now >= sSetterLast) return;   // 10 times a second is plenty: it shows them seconds later
    sSetterLast = now;
    void *inv = ReadStaticObj(N.invd_instance, N.InventaryData, "_instance");
    if (!inv) return;
    if (sSetterPinsOff == -2) {
        char tn[64];
        sSetterPinsOff = (FieldTypeName(N.InventaryData, "pinseterKegelRenderer", tn, sizeof(tn)) && !strcmp(tn, "UnityEngine.GameObject[]")) ? FieldOffset(N.InventaryData, "pinseterKegelRenderer") : -1;
        for (int i = 0; i < kTurnMax; i++) sSetterShown[i] = sMirrorShown[i] = -2;
    }
    Il2CppArray *pins = sSetterPinsOff >= 0x10 ? At<Il2CppArray *>(inv, sSetterPinsOff) : nullptr;
    if (!pins) return;
    void *mgr = sSetterMgr.get();
    if (!mgr && sFrame % 120 == 0) {
        Il2CppClass *k = FindClass("", "PinSetterManager");
        if (k && sSetterMirrorOff == -2) {
            char tn[64];
            sSetterMirrorOff = (FieldTypeName(k, "mirrorOnPinsetter", tn, sizeof(tn)) && !strcmp(tn, "UnityEngine.GameObject[]")) ? FieldOffset(k, "mirrorOnPinsetter") : -1;
        }
        if (k) { mgr = FirstAlive(FindAll(TypeOf(k))); sSetterMgr.set(mgr); }
    }
    Il2CppArray *mir = (mgr && sSetterMirrorOff >= 0x10) ? At<Il2CppArray *>(mgr, sSetterMirrorOff) : nullptr;
    int n = (int)Len(pins);
    if (n > kTurnMax) n = kTurnMax;
    for (int i = 0; i < n; i++) {
        int want = sTurnKeep ? TurnPredict(i) : -1;    // -1: the game's own model, untouched
        void *go = Elem(pins, i);
        if (want != sSetterShown[i] || go != sSetterGo[i]) {
            if (want < 0 && sSetterShown[i] == -2) sSetterShown[i] = -1;   // never touched: nothing to put back
            else {
                int r = SetterTwist(go, &sSetterGo[i], &sSetterBase[i], want * 22.5f + kSetterTwist, want < 0);
                if (r > 0) { sSetterShown[i] = want; sSetterSets++; }
                else if (r == 0) sSetterBusy++;
                else sSetterFails++;
            }
        }
        void *mgo = (mir && (size_t)i < Len(mir)) ? Elem(mir, i) : nullptr;
        if (mgo && (want != sMirrorShown[i] || mgo != sMirrorGo[i])) {
            if (want < 0 && sMirrorShown[i] == -2) sMirrorShown[i] = -1;
            else if (SetterTwist(mgo, &sMirrorGo[i], &sMirrorBase[i], kSetterTwist - want * 22.5f, want < 0) > 0) sMirrorShown[i] = want;
        }
    }
}

// ---- the sweeper's banner (everywhere, looks only) ----
// What the game has (1.907, scene level1, read from the files): the blue "BELMO" banner on the pin sweeper is a
// world-space Canvas, PinSpotter/animation_mashina/.../pinspotter_1:joint42/joint1/GrabliCaption/Canvas (about
// 198 canvas units per metre). Its layers sit at different depths (canvas z): Image (a dark plate) +7.3, Bar (the
// head-to-head timer) 0, and Logo 0 with Fan (the blue stars) 0, H2H (grey, head-to-head) -1 and Text (the
// letters) -3 under it (Logo is scaled 0.962). The sweeper bar around it (mesh polySurface2130, skinned to joint42
// and joint1) is a frame: front face at -4.95 around a window about x -122..122, y -10..11, a dark back panel at
// +4.45, end blocks reaching -7.3, and it bends during the sweep (joint1 turns up to about 12 degrees against
// joint42). So the banner sat recessed in the bar: on a device the bar covered both of its ends (B and O cut by
// straight lines, both layers), and at the back of the lane the back panel covered the whole blue layer while the
// letters, a little further forward, stayed. PinSpotterInfoPannel (jasonLogo = Logo, bar = Bar's Image) only
// switches the two on and off, so a position set on them stays.
// The fix: Logo and Bar are moved to z = -12, in front of every part of the bar (its most forward part, the end
// blocks, is at -7.3), and sized to the bar's window: x -122.2..122.2, y -10.0..11.3 (measured from the mesh).
static const float kBannerZ = -12.0f;
static Ref sBannerPanel;
static int sBannerLogoOff = -2, sBannerBarOff = -2;
static int sBannerSets = 0, sBannerFails = 0;

struct BFVec3 { float x, y, z; };

// Moves one banner layer unless it's already there. Returns 1 moved, 0 already there, -1 failed.
static int BannerPlace(void *tr, float h) {          // h: the layer's own rect height (canvas units)
    bool ok = false;
    Il2CppObject *b = Invoke(N.Tr_getLocalPos, tr, nullptr, &ok);
    if (!ok || !b) return -1;
    BFVec3 cur = *(BFVec3 *)Unbox(b);
    if (fabsf(cur.z - kBannerZ) < 0.01f) return 0;
    BFVec3 pos = { 0.0f, 0.65f, kBannerZ };
    BFVec3 scale = { 244.4f / 256.0f, 21.3f / h, 0.962f };    // (z scale kept: the Logo's layers keep their order)
    void *a[] = { &pos };
    Invoke(N.Tr_setLocalPos, tr, a, &ok);
    if (!ok) return -1;
    void *c[] = { &scale };
    Invoke(N.Tr_setLocalScale, tr, c, &ok);
    return ok ? 1 : -1;
}

static void SweeperBannerTick() {
    if (!N.Tr_getLocalPos || !N.Tr_setLocalPos || !N.Tr_setLocalScale || !N.GO_getTransform || !N.Comp_getTransform) return;
    if (sFrame % 60 != 0) return;
    void *panel = sBannerPanel.get();
    if (!panel) {
        Il2CppClass *k = FindClass("", "PinSpotterInfoPannel");
        if (!k) return;
        if (sBannerLogoOff == -2) {
            char tn[64];
            sBannerLogoOff = (FieldTypeName(k, "jasonLogo", tn, sizeof(tn)) && !strcmp(tn, "UnityEngine.GameObject")) ? FieldOffset(k, "jasonLogo") : -1;
            sBannerBarOff = (FieldTypeName(k, "bar", tn, sizeof(tn)) && !strcmp(tn, "UnityEngine.UI.Image")) ? FieldOffset(k, "bar") : -1;
        }
        panel = FirstAlive(FindAll(TypeOf(k)));
        if (!panel) return;
        sBannerPanel.set(panel);
    }
    void *logo = sBannerLogoOff >= 0x10 ? At<void *>(panel, sBannerLogoOff) : nullptr;
    void *bar = sBannerBarOff >= 0x10 ? At<void *>(panel, sBannerBarOff) : nullptr;
    bool ok = false;
    void *tr = Alive(logo) ? Invoke(N.GO_getTransform, logo, nullptr, &ok) : nullptr;
    int r = (ok && tr) ? BannerPlace(tr, 22.0f) : -1;
    if (r > 0) sBannerSets++; else if (r < 0 && logo) sBannerFails++;
    ok = false;
    tr = Alive(bar) ? Invoke(N.Comp_getTransform, bar, nullptr, &ok) : nullptr;
    r = (ok && tr) ? BannerPlace(tr, 23.0f) : -1;
    if (r > 0) sBannerSets++; else if (r < 0 && bar) sBannerFails++;
}

// Stands in for the engine's Random.RandomRangeInt. The game's Random.Range(int, int) jumps here with its
// caller's return address intact, so __builtin_return_address(0) is the call site in the game's code.
__attribute__((noinline)) static int32_t BFRandomRangeInt(int32_t lo, int32_t hi) {
    uintptr_t ra = (uintptr_t)__builtin_return_address(0);
    int32_t r = sRandEngine(lo, hi);                   // always the game's own draw, so its random sequence is unchanged
    if (lo == 0 && hi == 16) {
        for (int s = 0; s < sTurnSites; s++)
            if (ra == sTurnSite[s]) return TurnPick(r);
        sTurnOther++;
    }
    return r;
}

// The data pointer a Random.Range(int, int) / RandomRangeInt body loads the engine's function from:
//   adrp xN, page ... ldr x2, [xN, #off] ... br x2   (a tail call: the caller's return address is kept)
// Returns nullptr for anything else, including a body that returns instead of jumping.
static void *RandSlotIn(const uint32_t *code) {
    uintptr_t page[32];
    bool have[32];
    memset(have, 0, sizeof(have));
    void *slot = nullptr;
    for (int k = 0; k < 24; k++) {
        uint32_t w = code[k];
        uintptr_t pc = (uintptr_t)(code + k);
        if ((w & 0x9F000000u) == 0x90000000u) {                    // ADRP
            int64_t imm = (int64_t)((((w >> 5) & 0x7FFFFu) << 2) | ((w >> 29) & 3u));
            if (imm & (1 << 20)) imm -= (1 << 21);
            page[w & 31] = (uintptr_t)((int64_t)(pc & ~(uintptr_t)0xFFF) + imm * 4096);
            have[w & 31] = true;
        } else if ((w & 0xFFC0001Fu) == 0xF9400002u) {             // LDR X2, [Xn, #imm]
            int rn = (int)((w >> 5) & 31);
            slot = have[rn] ? (void *)(page[rn] + ((w >> 10) & 0xFFFu) * 8) : nullptr;
        } else if (w == 0xD61F0040u) {                             // BR X2
            return slot;
        } else if (w == 0xD65F03C0u) {                             // RET
            return nullptr;
        }
    }
    return nullptr;
}

// Return addresses of the Random.Range(0, 16) calls in UpdatePinPositions (the first 4 KB of it): a BL whose
// target loads the engine function from `slot`, with MOV W1, #16 among the 4 instructions before it.
static int FindTurnSites(const uint32_t *code, void *slot, uintptr_t *out, int max) {
    int n = 0;
    for (int k = 4; k < 1024 && n < max; k++) {
        uint32_t w = code[k];
        if ((w & 0xFC000000u) != 0x94000000u) continue;            // BL
        bool sixteen = false;
        for (int j = 1; j <= 4; j++) if (code[k - j] == 0x52800201u) sixteen = true;   // MOV W1, #16
        if (!sixteen) continue;
        int64_t imm = (int64_t)(w & 0x3FFFFFFu);
        if (imm & (1 << 25)) imm -= (1 << 26);
        const uint32_t *target = (const uint32_t *)((int64_t)(uintptr_t)(code + k) + imm * 4);
        if (RandSlotIn(target) == slot) out[n++] = (uintptr_t)(code + k + 1);
    }
    return n;
}

static void TurnHookInstall() {
    sTurnHook = -1;
    Il2CppClass *rnd = FindClass("UnityEngine", "Random");
    const MethodInfo *m = rnd ? FindMethod(rnd, "RandomRangeInt", 2) : nullptr;
    if (!m && rnd) m = FindMethod(rnd, "Range", 2, "System.Int32", "System.Int32");
    // MethodInfo starts with its code pointer (methodPointer)
    const uint32_t *rcode = m ? *(const uint32_t *const *)m : nullptr;
    const uint32_t *ucode = N.RPT_UpdatePinPositions ? *(const uint32_t *const *)N.RPT_UpdatePinPositions : nullptr;
    if (!rcode || !ucode) { sTurnWhy = "Random.Range or UpdatePinPositions not found"; return; }
    BFRandIntFn *slot = (BFRandIntFn *)RandSlotIn(rcode);
    if (!slot) { sTurnWhy = "Random.Range has an unexpected shape"; return; }
    sTurnSites = FindTurnSites(ucode, (void *)slot, sTurnSite, 4);
    if (!sTurnSites) { sTurnWhy = "no Random.Range(0, 16) in UpdatePinPositions"; return; }
    BFRandIntFn engine = *slot;                        // filled the first time the game drew a random number
    if (!engine) {
        typedef void *(*ResolveFn)(const char *);
        ResolveFn resolve = (ResolveFn)Api("il2cpp_resolve_icall");
        if (resolve) engine = (BFRandIntFn)resolve("UnityEngine.Random::RandomRangeInt(System.Int32,System.Int32)");
    }
    if (!engine || engine == BFRandomRangeInt) { sTurnWhy = "the engine's random function wasn't found"; return; }
    sRandEngine = engine;
    sRandSlot = slot;
    __atomic_store_n(slot, (BFRandIntFn)BFRandomRangeInt, __ATOMIC_RELEASE);
    sTurnHook = 1;
    sTurnWhy = "in place";
}

static void PinTurnTick() {
    if (!sTurnHook && sSettled && N.RPT_UpdatePinPositions) {
        TurnHookInstall();
        char msg[160];
        snprintf(msg, sizeof(msg), "%s (%d call site%s)", sTurnWhy, sTurnSites, sTurnSites == 1 ? "" : "s");
        BFLog(PIN_TURN_LOG, (const char *)msg);
    }
    sTurnKeep = sTurnHook == 1 && sSettled && sMode == MODE_FUN;   // Practice, its replays included
    if (sSettled) { PinsetterTurnTick(); SweeperBannerTick(); }
}

static void TurnDebug(char *buf, size_t size) {
    snprintf(buf, size, "pin turns: hook=%s sites=%d keep=%d kegels=%d | racks=%d kept=%d new=%d knocked=%d offLane=%d | other=%d upUnknown=%d leanFails=%d maxStandLean=%.1f | pinsetter: found=%d mirrors=%d sets=%d busy=%d fails=%d | sweeper banner: found=%d moved=%d fails=%d",
             sTurnWhy, sTurnSites, sTurnKeep ? 1 : 0, sTurnCount, sTurnRacks, sTurnKept, sTurnNew, sTurnKnocked, sTurnOffLane,
             sTurnOther, sTurnUpUnknown, sTurnTiltFails, sTurnMaxStandTilt, sSetterPinsOff >= 0x10 ? 1 : 0,
             sSetterMirrorOff >= 0x10 && sSetterMgr.get() ? 1 : 0, sSetterSets, sSetterBusy, sSetterFails,
             sBannerPanel.get() ? 1 : 0, sBannerSets, sBannerFails);
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
Str BFPinImagePath(void) {                    // files/BowlingPlus/pin_image.png (Documents/BowlingPlus on iOS)
    return BFDataDir() + "/pin_image.png";
}

static uintptr_t sPinImgTex = 0;
static double sPinImgStamp = -1;
static int sPinImgSize = 0, sPinImgFails = 0, sPinImgMats = 0;
static const int kPinMatMax = 16;
// (sInvData is declared with the pin-turn code above)
static ObjRef sPinMat[kPinMatMax];                // materials we changed
static uintptr_t sPinMatOrig[kPinMatMax];         // what each one showed before (strong handles)
static int sPinMatCount = 0;

static void *PinImageTexture() {
    Str path = BFPinImagePath();
    double stamp = 0;
    bool have = FileMTime(path, stamp);
    void *t = sPinImgTex ? Target(sPinImgTex) : nullptr;
    if (Alive(t) && have && stamp == sPinImgStamp) return t;
    if (!have || sPinImgFails >= 3 || !N.Texture2D || !N.T2D_ctor || !N.LoadImage || !N.Byte) return nullptr;
    std::vector<uint8_t> png;
    if (!ReadFile(path, png) || png.empty()) return nullptr;
    Il2CppObject *tex = NewObject(N.Texture2D);
    int w = 2, h = 2;
    bool ok = false;
    void *ca[] = { &w, &h };
    if (tex) Invoke(N.T2D_ctor, tex, ca, &ok);
    Il2CppArray *bytes = ok ? NewArray(N.Byte, png.size()) : nullptr;
    if (bytes) memcpy(Data(bytes), png.data(), png.size());
    void *la[] = { tex, bytes };
    if (!bytes || !InvokeBool(N.LoadImage, nullptr, la, false)) { sPinImgFails++; BFLog("pin image: couldn't load it"); return nullptr; }
    DontUnload(tex);
    if (sPinImgTex) Release(sPinImgTex);
    sPinImgTex = Keep(tex);
    sPinImgStamp = stamp;
    sPinImgFails = 0;
    sPinImgSize = png.size() > 24 ? (int)((uint32_t)png[16] << 24 | (uint32_t)png[17] << 16 | (uint32_t)png[18] << 8 | png[19]) : 0;   // PNG width
    BFLog("pin image loaded (%d px)", sPinImgSize);
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
        if (full && sPinMatCount) { PinImageRestore(); BFLog("pin image off: the game's pins are back"); }
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

Str BFPinImageStatus(void) {
    double m = 0;
    bool have = FileMTime(BFPinImagePath(), m);
    if (!have) return "No image yet. Get the wrap template, draw your design on it (2:1), then pick it.";
    if (!gBF.pinImage) return "Your image is saved. Turn the switch on to use it.";
    if (sPinImgFails >= 3) return "Couldn't load that image. Try picking it again (PNG or JPEG).";
    if (sPinImgMats > 0) return Fmt("On the pins now (%d px image).", sPinImgSize);
    return "Shows on the pins when a lane is on screen.";
}

static void BallCCDTick() {   // continuous collision for the ball, while realistic pin physics or a speed boost is on
    if (sFrame % 15 != 0) return;
    bool want = gBFStatus.offline && (gBF.pinPhys || gBF.speedMult > 1.5f);
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
    BFLog("ball speed x%.1f (%.1f -> %.1f m/s)", mult, speed, speed * mult);
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
    BFLog("ball grip back to the game's (rpmFactor %.3f)", sRpmFactorOrig);
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
    BFLog("ball spin x%.1f (%d -> %d rpm), grip x%.2f", mult, sSpunFrom, sSpunTo, sGrip);
    // the scoreboard's "599 rpm": show the boosted number
    void *rpt = sRPT.get();
    Il2CppArray *texts = (rpt && N.rpt_rpmText >= 0) ? At<Il2CppArray *>(rpt, N.rpt_rpmText) : nullptr;
    for (size_t i = 0; i < Len(texts) && N.Txt_get && N.Txt_set; i++) {
        void *t = Elem(texts, i);
        if (!Alive(t)) continue;
        Str cur = Text((Il2CppString *)Invoke(N.Txt_get, t, nullptr));
        int shown = StrInt(cur);
        if (shown <= 0) continue;
        size_t cut = cur.find_first_not_of("0123456789");
        Str rest = cut == Str::npos ? Str() : cur.substr(cut);
        void *ta[] = { NewString(Fmt("%d%s", (int)lroundf(shown * mult), rest).c_str()) };
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
    BFLog("spare mode: racked pins mask 0x%03x", mask);
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
static const char *kPearlTexName = "Text_MatchupPearl_MatchupHybrid";
static const char *kPearlSheetKey = "matchuppearl";   // in the Pearl/Hybrid file's name
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
static double sPearlNextTry = 0;

static std::unordered_map<int, Str> sNames;
static Str sBallLine;
static void *sLastItem = nullptr;
static Skin sCurSkin = SKIN_NONE;

static Skin SkinForName(const Str &name) {
    Str n = Squash(name);
    if (!StrHasPrefix(n, "matchup")) return SKIN_NONE;
    if (n == "matchuppearl") return SKIN_PEARL;
    if (StrHas(n, "black") || StrHasSuffix(n, "bp")) return SKIN_BP;
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

static Str ItemName(void *item) {
    if (!item || N.shop_name < 0) return Str();
    int key = N.item_itemId >= 0 ? At<int>(item, N.item_itemId) : 0;
    auto it = sNames.find(key);
    if (it != sNames.end()) return it->second;
    void *si = ShopItemOf(item);
    Str name = si ? Text(At<void *>(si, N.shop_name)) : Str();
    if (!name.empty()) sNames[key] = name;
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

static Str DlcPath(int id) {                 // the skin file's path inside the game's asset bundles
    if (id < 0 || !N.IVD_GetItemDataByID) return Str();
    void *v = VisualData();
    if (!v) return Str();
    void *a[] = { &id };
    void *rec = Invoke(N.IVD_GetItemDataByID, v, a);   // Models.mdl_DLS_Item
    if (!rec) return Str();
    static int urlOff = -2;
    if (urlOff == -2) urlOff = FieldOffset(ClassOf(rec), "dlc_url");
    return urlOff >= 0 ? Text(At<void *>(rec, urlOff)) : Str();
}

static const std::set<Str> &AppAssetNames() {   // file names of every asset shipped in the app's bundles
    static std::set<Str> names;
    static bool done = false;
    if (done) return names;
    Str txt = BFAppAssetList();                 // Android: assets/AssetBundles/**/*.manifest inside the APK (read by Java)
    if (txt.empty()) return names;              // not readable yet: try again next time
    done = true;
    for (const Str &raw : StrSplit(txt, "\n")) {
        Str line = StrTrim(raw);
        if (!StrHas(line, "- Assets/")) continue;
        names.insert(StrLower(StrDelExt(StrLastPath(line))));
    }
    return names;
}

static int Shipped(int id) {                 // 1 = its skin file ships with the app, 0 = it doesn't, -1 = can't tell
    Str p = DlcPath(id);
    const std::set<Str> &names = AppAssetNames();
    if (p.empty() || names.empty()) return -1;
    return names.count(StrLower(StrDelExt(StrLastPath(p)))) ? 1 : 0;
}

// Two-ball files are named after both balls, first ball = left half = texc 0 (checked on device:
// Pearl texc 0 / Hybrid texc 1 on text_matchuppearl_matchuphybrid). Returns -1 if `key` isn't in the name.
static int HalfOf(const Str &path, const Str &key) {
    Str stem = StrLower(StrDelExt(StrLastPath(path)));
    std::vector<Str> parts = StrSplit(stem, "_");
    while (!parts.empty() && (parts[0] == "tex" || parts[0] == "text")) parts.erase(parts.begin());
    for (size_t i = 0; i < parts.size(); i++)
        if (StrHas(Squash(parts[i]), key)) return i == 0 ? 0 : 1;
    return -1;
}

static bool PathHas(const Str &path, const Str &key) { return !path.empty() && StrHas(Squash(path), key); }

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
            BFLog("skin fix: off, the game's own links are back");
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
        Str n = Squash(Text(At<void *>(it, N.shop_name)));
        if (!StrHasPrefix(n, "matchup")) continue;
        if (n == "matchuppearl") pearl = it;
        else if (n == "matchuphybrid") hybrid = it;
        else if (n == "matchupsolid") solid = it;
        else if (StrHas(n, "black") || StrHasSuffix(n, "bp")) bp = it;
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
        Str pPath = DlcPath(pd), hPath = DlcPath(hd);
        if (pd >= 0 && PathHas(pPath, kPearlSheetKey)) sPearlSheet = pd;            // right file, wrong half?
        else if (hd >= 0 && (hPath.empty() || PathHas(hPath, kPearlSheetKey))) sPearlSheet = hd;
        if (sPearlSheet < 0) {
            sPearlFail = PF_NO_SHEET;
            BFLog("skin fix: no Pearl/Hybrid file found (pearl %d %s, hybrid %d %s)", pd, pPath, hd, hPath);
        } else {
            // the Pearl is the left half; the Hybrid (right half) is the reference when it shares the file
            int want = (sPearlSheet == hd && ht >= 0) ? (ht ? 0 : 1) : 0;
            if (SkinDlcOf(pearl) == sPearlSheet && TexcOf(pearl) == want) sPearlFixed = true;
            else {
                sPearlFixed = SetLink(pearl, sPearlSheet, want, sPearlLink);
                sNeedRackRefresh = sNeedHandReload = true;
            }
            sPearlFail = sPearlFixed ? PF_OK : PF_LINK_FAILED;
            BFLog("skin fix: Pearl -> skin %d half %d (game had %d/%d %s) ok=%d", sPearlSheet, want, pd, pt, pPath.empty() ? Str("-") : pPath, sPearlFixed);
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
            Str path = DlcPath(c);
            if (!PathHas(path, "blackpearl") || Shipped(c) == 0) continue;
            int h = HalfOf(path, "blackpearl");
            target = c;
            half = h >= 0 ? h : 0;
            break;
        }
        if (target < 0 && bs == -1 && pd >= 0 && pd != sPearlSheet) { target = pd; half = pt == 1 ? 1 : 0; }   // catalog unreadable
        if (target >= 0) {
            sBPFixed = SetLink(bp, target, half, sBPLink);
            sNeedRackRefresh = sNeedHandReload = true;
            BFLog("skin fix: BP skin %d (shipped=%d) -> %d half %d ok=%d", bd, bs, target, half, sBPFixed);
        }
    }
    sBPHasSkin = bs == 1 || sBPFixed;
}

static void CurrentBallTick() {
    if (!sSettled || sFrame % 20 != 0) return;
    // the game's own cached "current ball" (its getter can make the game load things early)
    void *item = ReadStaticObj(N.inv_currentBall, N.InvHelper, "_currentBall");
    if (item == sLastItem) return;
    Str nm = ItemName(item);
    sLastItem = !nm.empty() ? item : nullptr;   // retry later if the store data isn't ready yet
    sCurSkin = SkinForName(nm);
    sBallLine = !nm.empty() ? "Current ball: " + nm : Str();
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
    BFLog("skin fix: refreshed the ball rack");
}

#include <sys/stat.h>
#define BG_IMAGE_PATH BFBgImagePath()
#define BG_LOG "alley background: %s"
#define BF_UTF8(s) Text(s)
Str BFBgImagePath(void) { return BFDataDir() + "/bg_image.png"; }   // files/BowlingPlus/bg_image.png (Documents/BowlingPlus on iOS)
// ---- engine methods for the two parts below, found by full signature on first use ----
struct BFExtra {
    bool tried;
    Il2CppClass *Image, *Sprite, *RenderTexture, *GL, *Graphic, *Material, *Texture, *Object, *Transform, *GameObject;
    const MethodInfo *img_setOverride, *sp_create, *rt_ctor, *rt_getActive, *rt_setActive, *rt_release;
    const MethodInfo *gl_push, *gl_pop, *gl_ortho, *gl_begin, *gl_end, *gl_tex, *gl_vert, *gl_clear, *gl_color;
    const MethodInfo *gr_defaultMat, *mat_ctor, *mat_setPass, *mat_setTex, *mat_setInt, *gr_getMat, *gr_setMat;
    const MethodInfo *tex_w, *tex_h, *t2d_read, *encodePng, *obj_destroy, *obj_name, *tr_parent, *tr_find, *tr_childCount, *tr_child;
    const MethodInfo *comp_go, *go_setActive, *go_activeSelf, *go_getComponent, *tr_getPos;
};
static BFExtra X;

static const MethodInfo *XM(Il2CppClass *k, const char *name, int argc, const char *t0 = nullptr, const char *t1 = nullptr,
                            const char *t2 = nullptr, const char *t3 = nullptr) {
    if (!k) return nullptr;
    const char *types[4] = { t0, t1, t2, t3 };
    return FindMethodSig(k, name, argc, types);
}

static void ExtraResolve() {
    if (X.tried) return;
    X.tried = true;
    X.Image = FindClass("UnityEngine.UI", "Image");
    X.Graphic = FindClass("UnityEngine.UI", "Graphic");
    X.Sprite = FindClass("UnityEngine", "Sprite");
    X.RenderTexture = FindClass("UnityEngine", "RenderTexture");
    X.GL = FindClass("UnityEngine", "GL");
    X.Material = FindClass("UnityEngine", "Material");
    X.Texture = FindClass("UnityEngine", "Texture");
    X.Object = FindClass("UnityEngine", "Object");
    X.Transform = FindClass("UnityEngine", "Transform");
    X.GameObject = FindClass("UnityEngine", "GameObject");
    Il2CppClass *conv = FindClass("UnityEngine", "ImageConversion");
    Il2CppClass *t2d = FindClass("UnityEngine", "Texture2D");
    X.img_setOverride = XM(X.Image, "set_overrideSprite", 1);
    X.sp_create = XM(X.Sprite, "Create", 3, "UnityEngine.Texture2D", "UnityEngine.Rect", "UnityEngine.Vector2");
    X.rt_ctor = XM(X.RenderTexture, ".ctor", 4, "System.Int32", "System.Int32", "System.Int32", "UnityEngine.RenderTextureFormat");
    X.rt_getActive = XM(X.RenderTexture, "get_active", 0);
    X.rt_setActive = XM(X.RenderTexture, "set_active", 1);
    X.rt_release = XM(X.RenderTexture, "Release", 0);
    X.gl_push = XM(X.GL, "PushMatrix", 0);
    X.gl_pop = XM(X.GL, "PopMatrix", 0);
    X.gl_ortho = XM(X.GL, "LoadOrtho", 0);
    X.gl_begin = XM(X.GL, "Begin", 1, "System.Int32");
    X.gl_end = XM(X.GL, "End", 0);
    X.gl_tex = XM(X.GL, "TexCoord2", 2, "System.Single", "System.Single");
    X.gl_vert = XM(X.GL, "Vertex3", 3, "System.Single", "System.Single", "System.Single");
    X.gl_clear = XM(X.GL, "Clear", 3, "System.Boolean", "System.Boolean", "UnityEngine.Color");
    X.gl_color = XM(X.GL, "Color", 1, "UnityEngine.Color");
    X.gr_defaultMat = XM(X.Graphic, "get_defaultGraphicMaterial", 0);
    X.gr_getMat = XM(X.Graphic, "get_material", 0);
    X.gr_setMat = XM(X.Graphic, "set_material", 1);
    X.mat_ctor = XM(X.Material, ".ctor", 1, "UnityEngine.Material");
    X.mat_setPass = XM(X.Material, "SetPass", 1, "System.Int32");
    X.mat_setTex = XM(X.Material, "set_mainTexture", 1);
    X.mat_setInt = XM(X.Material, "SetInt", 2, "System.String", "System.Int32");
    X.tex_w = XM(X.Texture, "get_width", 0);
    X.tex_h = XM(X.Texture, "get_height", 0);
    X.t2d_read = XM(t2d, "ReadPixels", 3, "UnityEngine.Rect", "System.Int32", "System.Int32");
    X.encodePng = XM(conv, "EncodeToPNG", 1, "UnityEngine.Texture2D");
    X.obj_destroy = XM(X.Object, "Destroy", 1, "UnityEngine.Object");
    X.obj_name = XM(X.Object, "get_name", 0);
    X.tr_parent = XM(X.Transform, "get_parent", 0);
    X.tr_find = XM(X.Transform, "Find", 1, "System.String");
    X.tr_childCount = XM(X.Transform, "get_childCount", 0);
    X.tr_child = XM(X.Transform, "GetChild", 1, "System.Int32");
    X.tr_getPos = XM(X.Transform, "get_position", 0);
    X.comp_go = XM(FindClass("UnityEngine", "Component"), "get_gameObject", 0);
    X.go_setActive = XM(X.GameObject, "SetActive", 1, "System.Boolean");
    X.go_activeSelf = XM(X.GameObject, "get_activeSelf", 0);
    X.go_getComponent = XM(X.GameObject, "GetComponent", 1, "System.Type");
}

struct BFColor { float r, g, b, a; };
struct BFRect { float x, y, w, h; };
struct BFVec2 { float x, y; };

static bool NameStarts(void *obj, const char *prefix) {
    bool ok = false;
    Il2CppString *s = X.obj_name ? (Il2CppString *)Invoke(X.obj_name, obj, nullptr, &ok) : nullptr;
    if (!ok || !s) return false;
    std::string n = BF_UTF8(s);
    return n.compare(0, strlen(prefix), prefix) == 0;
}

// ---- the game's own pin picture, for the pin library's "Off" preview (looks only) ----
// What the game has (1.907): the pin pictures (pins/fulltextures, 178 of them, 512 x 512) are compressed and not
// readable, and this build has no Graphics.Blit or RenderTexture.GetTemporary. So BowlingPlus draws the picture into
// a RenderTexture itself with GL immediate mode (a copy of the UI material, which shows _MainTex as it is), reads it
// back (Texture2D.ReadPixels) and makes a PNG (ImageConversion.EncodeToPNG). Which picture: the pins' material
// (InventaryData's pin material) shows the game's own, unless a custom picture is on, then it's the original
// BowlingPlus kept for that material.
static const char *sGamePinWhy = "not asked yet";
static int sGamePinSize = 0;

static bool GamePinPNG(std::vector<uint8_t> &png) {
    ExtraResolve();
    png.clear();
    if (!X.rt_ctor || !X.rt_setActive || !X.gl_begin || !X.gl_vert || !X.gl_tex || !X.gr_defaultMat || !X.mat_ctor ||
        !X.mat_setPass || !X.mat_setTex || !X.t2d_read || !X.encodePng || !N.T2D_ctor || !N.Texture2D || !N.M_getMainTex) {
        sGamePinWhy = "a method is missing";
        return false;
    }
    void *inv = ReadStaticObj(N.invd_instance, N.InventaryData, "_instance");
    void *mat = inv && N.inv_pinMat >= 0 ? At<void *>(inv, N.inv_pinMat) : nullptr;
    if (!Alive(mat)) { sGamePinWhy = "no pin material"; return false; }
    int mi = PinMatIndex(mat);
    void *tex = mi >= 0 && sPinMatOrig[mi] ? Target(sPinMatOrig[mi]) : (void *)Invoke(N.M_getMainTex, mat, nullptr);
    if (!Alive(tex)) { sGamePinWhy = "no pin picture"; return false; }
    int w = InvokeInt(X.tex_w, tex, nullptr, 512), h = InvokeInt(X.tex_h, tex, nullptr, 512);
    if (w < 8 || h < 8 || w > 2048 || h > 2048) { w = 512; h = 512; }
    bool ok = false;
    Il2CppObject *rt = NewObject(X.RenderTexture);
    int zero = 0, fmt = 0;                                   // no depth, RenderTextureFormat.ARGB32
    void *ra[] = { &w, &h, &zero, &fmt };
    if (rt) Invoke(X.rt_ctor, rt, ra, &ok);
    if (!ok) { sGamePinWhy = "couldn't make the render texture"; return false; }
    void *base = (void *)Invoke(X.gr_defaultMat, nullptr, nullptr, &ok);
    Il2CppObject *m = (ok && base) ? NewObject(X.Material) : nullptr;
    void *ma[] = { base };
    ok = false;
    if (m) Invoke(X.mat_ctor, m, ma, &ok);
    if (!ok) { sGamePinWhy = "couldn't make the material"; return false; }
    void *ta[] = { tex };
    Invoke(X.mat_setTex, m, ta);
    void *prev = X.rt_getActive ? (void *)Invoke(X.rt_getActive, nullptr, nullptr) : nullptr;
    void *sa[] = { rt };
    Invoke(X.rt_setActive, nullptr, sa);
    if (X.gl_push) Invoke(X.gl_push, nullptr, nullptr);
    if (X.gl_ortho) Invoke(X.gl_ortho, nullptr, nullptr);
    BFColor white = { 1, 1, 1, 1 };
    bool yes = true;
    void *ca[] = { &yes, &yes, &white };
    if (X.gl_clear) Invoke(X.gl_clear, nullptr, ca);
    int pass = 0;
    void *pa[] = { &pass };
    Invoke(X.mat_setPass, m, pa);
    int quads = 7;                                           // GL.QUADS
    void *ba[] = { &quads };
    Invoke(X.gl_begin, nullptr, ba);
    void *wa[] = { &white };
    if (X.gl_color) Invoke(X.gl_color, nullptr, wa);
    const float q[4][2] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 0 } };
    float zz = 0;
    for (int k = 0; k < 4; k++) {
        float u = q[k][0], v = q[k][1];
        void *tc[] = { &u, &v };
        Invoke(X.gl_tex, nullptr, tc);
        void *vc[] = { &u, &v, &zz };
        Invoke(X.gl_vert, nullptr, vc);
    }
    Invoke(X.gl_end, nullptr, nullptr);
    if (X.gl_pop) Invoke(X.gl_pop, nullptr, nullptr);
    Il2CppObject *t2 = NewObject(N.Texture2D);
    void *da[] = { &w, &h };
    ok = false;
    if (t2) Invoke(N.T2D_ctor, t2, da, &ok);
    BFRect r = { 0, 0, (float)w, (float)h };
    void *rd[] = { &r, &zero, &zero };
    bool read = false;
    if (ok) Invoke(X.t2d_read, t2, rd, &read);
    void *pv[] = { prev };
    Invoke(X.rt_setActive, nullptr, pv);
    Il2CppArray *bytes = nullptr;
    if (read) {
        void *ea[] = { t2 };
        bytes = (Il2CppArray *)Invoke(X.encodePng, nullptr, ea, &ok);
    }
    if (bytes && Len(bytes)) png.assign((uint8_t *)Data(bytes), (uint8_t *)Data(bytes) + Len(bytes));
    if (X.rt_release) Invoke(X.rt_release, rt, nullptr);
    if (X.obj_destroy) {
        void *d1[] = { t2 }, *d2[] = { m }, *d3[] = { rt };
        if (t2) Invoke(X.obj_destroy, nullptr, d1);
        Invoke(X.obj_destroy, nullptr, d2);
        Invoke(X.obj_destroy, nullptr, d3);
    }
    sGamePinSize = w;
    sGamePinWhy = png.empty() ? "reading it back failed" : "ok";
    return !png.empty();
}

// ---- your own alley background (looks only) ----
// What the game has (1.907, scene level1): the picture behind the lanes is a world-space canvas,
// ObjectsToShift/Banners (2820 x 850: three Images OBJ_BG_banner_1..3_940x850 side by side, no gaps), with a copy for
// the floor reflection (ObjectsToShift/BannersMirror) and one more set (Banners). Each room puts its own sprite on
// them (Orange Tenpin Bowl: Banner_room_back_Orange_back) and shows its name from Banners/TitleSubcanvas
// (OBJ_headline_Orange and the others). BowlingPlus loads bg_image.png (already fitted to the wall's 2820:850 by the
// menu) and puts a third of it on each panel as Image.overrideSprite, so the game's own sprites stay underneath and
// come back as soon as it's off. While it's on, the room's name is hidden too, unless "Show the alley name" is on.
static GCHandle sBgTex = 0, sBgSprite[3] = { 0, 0, 0 };
static double sBgStamp = -1;
static int sBgW = 0, sBgH = 0, sBgFails = 0, sBgPanels = 0, sBgTitles = 0;
static const int kBgMax = 24;
static Ref sBgPanel[kBgMax];
static int sBgPanelIdx[kBgMax];
static Ref sBgTitle[4];                                       // the TitleSubcanvas objects we hid
static int sBgPanelCount = 0, sBgTitleCount = 0;
static bool sBgApplied = false;

static bool BgFileStamp(const std::string &path, double &stamp) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    stamp = (double)st.st_mtime + st.st_size * 1e-9;
    return true;
}

static void BgDropTexture() {
    for (int i = 0; i < 3; i++) if (sBgSprite[i]) { Release(sBgSprite[i]); sBgSprite[i] = 0; }
    if (sBgTex) { Release(sBgTex); sBgTex = 0; }
}

static bool BgLoad() {                                        // bg_image.png -> texture + 3 sprites (when it changed)
    std::string path = BG_IMAGE_PATH;
    double stamp = 0;
    if (!BgFileStamp(path, stamp)) return false;
    if (sBgTex && stamp == sBgStamp && Alive(Target(sBgTex))) return true;
    if (sBgFails >= 3 || !N.Texture2D || !N.T2D_ctor || !N.LoadImage || !N.Byte || !X.sp_create) return false;
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> png;
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) png.insert(png.end(), buf, buf + n);
    fclose(f);
    if (png.empty()) return false;
    Il2CppObject *tex = NewObject(N.Texture2D);
    int w = 2, h = 2;
    bool ok = false;
    void *ca[] = { &w, &h };
    if (tex) Invoke(N.T2D_ctor, tex, ca, &ok);
    Il2CppArray *bytes = ok ? NewArray(N.Byte, png.size()) : nullptr;
    if (bytes) memcpy(Data(bytes), png.data(), png.size());
    void *la[] = { tex, bytes };
    if (!bytes || !InvokeBool(N.LoadImage, nullptr, la, false)) { sBgFails++; BFLog(BG_LOG, "couldn't load the picture"); return false; }
    DontUnload(tex);
    w = InvokeInt(X.tex_w, tex, nullptr, 0);
    h = InvokeInt(X.tex_h, tex, nullptr, 0);
    if (w < 3 || h < 1) { sBgFails++; return false; }
    BgDropTexture();
    sBgTex = Keep(tex);
    for (int i = 0; i < 3; i++) {                            // a third of the picture for each panel, left to right
        BFRect r = { (float)(w * i / 3), 0, (float)(w * (i + 1) / 3 - w * i / 3), (float)h };
        BFVec2 pivot = { 0.5f, 0.5f };
        void *sa[] = { tex, &r, &pivot };
        void *sp = (void *)Invoke(X.sp_create, nullptr, sa, &ok);
        if (ok && sp) { DontUnload(sp); sBgSprite[i] = Keep(sp); }
    }
    sBgStamp = stamp;
    sBgW = w;
    sBgH = h;
    sBgFails = 0;
    sBgApplied = false;                                       // put the new one on
    BFLog(BG_LOG, "picture loaded");
    return true;
}

// The panels (OBJ_BG_banner_<n>_940x850 Images) and their TitleSubcanvas, found once and kept.
static void BgFindPanels() {
    sBgPanelCount = 0;
    sBgTitleCount = 0;
    if (!X.Image || !X.obj_name) return;
    Il2CppArray *all = FindAll(TypeOf(X.Image));
    for (size_t i = 0; all && i < Len(all) && sBgPanelCount < kBgMax; i++) {
        void *img = Elem(all, i);
        if (!Alive(img) || !NameStarts(img, "OBJ_BG_banner_")) continue;
        bool ok = false;
        Il2CppString *s = (Il2CppString *)Invoke(X.obj_name, img, nullptr, &ok);
        std::string n = ok && s ? BF_UTF8(s) : std::string();
        int idx = n.size() > 14 ? n[14] - '1' : -1;            // OBJ_BG_banner_1_..: 0, 1, 2 from the left
        if (idx < 0 || idx > 2) continue;
        sBgPanel[sBgPanelCount].set(img);
        sBgPanelIdx[sBgPanelCount] = idx;
        sBgPanelCount++;
        // its wall's room name: <wall>/TitleSubcanvas
        void *tr = Invoke(N.Comp_getTransform, img, nullptr, &ok);
        void *wall = ok && tr && X.tr_parent ? (void *)Invoke(X.tr_parent, tr, nullptr, &ok) : nullptr;
        if (ok && wall && X.tr_find && sBgTitleCount < 4) {
            void *fa[] = { NewString("TitleSubcanvas") };
            void *title = (void *)Invoke(X.tr_find, wall, fa, &ok);
            void *go = ok && title && X.comp_go ? (void *)Invoke(X.comp_go, title, nullptr, &ok) : nullptr;
            bool known = false;
            for (int k = 0; k < sBgTitleCount; k++) if (sBgTitle[k].get() == go) known = true;
            if (ok && go && !known) sBgTitle[sBgTitleCount++].set(go);
        }
    }
    sBgPanels = sBgPanelCount;
    sBgTitles = sBgTitleCount;
}

static void BgShowTitles(bool show) {
    if (!X.go_setActive) return;
    for (int k = 0; k < sBgTitleCount; k++) {
        void *go = sBgTitle[k].get();
        if (!Alive(go)) continue;
        void *a[] = { &show };
        Invoke(X.go_setActive, go, a);
    }
}

static void BgApply(bool on) {
    if (!X.img_setOverride) return;
    for (int i = 0; i < sBgPanelCount; i++) {
        void *img = sBgPanel[i].get();
        if (!Alive(img)) continue;
        void *sp = on && sBgSprite[sBgPanelIdx[i]] ? Target(sBgSprite[sBgPanelIdx[i]]) : nullptr;
        void *a[] = { sp };
        Invoke(X.img_setOverride, img, a);
    }
    BgShowTitles(!on || gBF.bgTitle);
    sBgApplied = on;
}

static bool sBgTitleShown = true;
static void BgTick() {
    if (!sSettled || sFrame % 30 != 0) return;
    ExtraResolve();
    bool want = gBF.bgImage && BgLoad();
    bool lost = false;
    for (int i = 0; i < sBgPanelCount; i++) if (!Alive(sBgPanel[i].get())) lost = true;
    if (want && (!sBgPanelCount || lost) && sFrame % 300 == 0) { BgFindPanels(); sBgApplied = false; }
    if (want && (!sBgApplied || sBgTitleShown != gBF.bgTitle)) { BgApply(true); sBgTitleShown = gBF.bgTitle; }
    else if (!want && sBgApplied) { BgApply(false); BFLog(BG_LOG, "off: the room's own picture is back"); }
}

static void BgDebug(char *buf, size_t size) {
    snprintf(buf, size, "alley background: on=%d size=%dx%d panels=%d titles=%d applied=%d fails=%d | game pin picture: %s (%d px)",
             gBF.bgImage ? 1 : 0, sBgW, sBgH, sBgPanels, sBgTitles, sBgApplied ? 1 : 0, sBgFails, sGamePinWhy, sGamePinSize);
}

// ---- the sweeper's banner stays in front of the bar (everywhere, looks only) ----
// After 1.6.8 put the banner in front of every part of the bar (as the scene data has it), a device still showed the
// bar's frame over both ends of it: blue and letters cut by the same straight lines, at the same place as before.
// Nothing in the scene's geometry, masks, materials or cameras explains it, and the bar (opaque) is drawn before the
// banner (world-space UI), so it can only hide it through the depth test. So while the sweeper is down in front of
// the pins (the banner less than 0.30 m above the lane; nothing is in front of it then), the banner's images draw
// without the depth test: a copy of the UI material with unity_GUIZTestMode = Always (8). Raised, they go back to
// the game's own material, so the banner can't show through anything above.
static Ref sTopMat;
static Ref sTopImg[6];
static int sTopCount = 0, sTopOn = -1, sTopSets = 0;

static void BannerTopFind(void *logo, void *bar) {
    sTopCount = 0;
    bool ok = false;
    void *tr = Alive(logo) && N.GO_getTransform ? (void *)Invoke(N.GO_getTransform, logo, nullptr, &ok) : nullptr;
    int n = ok && tr ? InvokeInt(X.tr_childCount, tr, nullptr, 0) : 0;
    for (int i = 0; i < n && i < 5; i++) {                   // Fan, H2H, Text
        void *ia[] = { &i };
        void *ch = (void *)Invoke(X.tr_child, tr, ia, &ok);
        void *go = ok && ch ? (void *)Invoke(X.comp_go, ch, nullptr, &ok) : nullptr;
        void *ga[] = { TypeOf(X.Image) };
        void *img = ok && go ? (void *)Invoke(X.go_getComponent, go, ga, &ok) : nullptr;
        if (ok && Alive(img)) sTopImg[sTopCount++].set(img);
    }
    if (Alive(bar)) sTopImg[sTopCount++].set(bar);
}

static void BannerTopTick() {
    if (!sSettled) return;
    void *panel = sBannerPanel.get();
    if (!Alive(panel) || sBannerLogoOff < 0x10) return;
    ExtraResolve();
    if (!X.gr_setMat || !X.mat_ctor || !X.mat_setInt || !X.gr_defaultMat || !X.tr_getPos || !X.tr_child || !X.go_getComponent || !X.comp_go) return;
    bool ok = false;
    void *tr = (void *)Invoke(N.Comp_getTransform, panel, nullptr, &ok);
    Il2CppObject *pb = ok && tr ? Invoke(X.tr_getPos, tr, nullptr, &ok) : nullptr;
    if (!ok || !pb) return;
    float height = ((BFVec3 *)Unbox(pb))->z;                // world up is +Z
    int want = height < 0.30f ? 1 : 0;
    if (want == sTopOn && sFrame % 120 != 0) return;
    if (!sTopCount || sFrame % 120 == 0) {
        void *logo = At<void *>(panel, sBannerLogoOff);
        void *bar = sBannerBarOff >= 0x10 ? At<void *>(panel, sBannerBarOff) : nullptr;
        BannerTopFind(logo, bar);
    }
    void *mat = sTopMat.get();
    if (want && !Alive(mat)) {
        void *base = (void *)Invoke(X.gr_defaultMat, nullptr, nullptr, &ok);
        Il2CppObject *m = ok && base ? NewObject(X.Material) : nullptr;
        void *ma[] = { base };
        ok = false;
        if (m) Invoke(X.mat_ctor, m, ma, &ok);
        if (!ok) return;
        int always = 8;                                       // CompareFunction.Always
        void *sa[] = { NewString("unity_GUIZTestMode"), &always };
        Invoke(X.mat_setInt, m, sa);
        DontUnload(m);
        sTopMat.set(m);
        mat = m;
    }
    for (int i = 0; i < sTopCount; i++) {
        void *img = sTopImg[i].get();
        if (!Alive(img)) continue;
        void *cur = X.gr_getMat ? (void *)Invoke(X.gr_getMat, img, nullptr) : nullptr;
        bool isTop = cur && cur == mat;
        if (want && !isTop) { void *a[] = { mat }; Invoke(X.gr_setMat, img, a); sTopSets++; }
        else if (!want && isTop) { void *a[] = { nullptr }; Invoke(X.gr_setMat, img, a); sTopSets++; }
    }
    sTopOn = want;
}

#define PP_LOG "pin physics: %s"
// ---- realistic pin physics (Practice) ----
// What the game has (1.907, scene level1 + code; see VERIFIED_NOTES 5f and tools/dev/pinlab): the pins on the lane are
// PinsPhys/PinUnity1..10 (InventaryData.kegels), each with seven colliders (five convex hulls, two capsules) whose
// physics materials "Pin" / "PinButtom" have friction 0.5 dynamic / 0.3 static, combined by Minimum with whatever
// they touch. InventaryData.ResetRigidBody rebuilds every pin's Rigidbody at each rack (DestroyImmediate + AddComponent,
// then maxDepenetrationVelocity and maxAngularVelocity 100000); the colliders and their materials stay.
// A PhysX 4.1 model of exactly that setup (tools/dev/pinlab), run through the US Bowling Congress's Bowlscore test
// (23 offsets x 11 entry angles), strikes 25% of the time where real pins strike about 42-44%, and entry angle makes no
// difference. With the pins' friction at 0.25 it strikes 37%, entry angle matters again (31% at 0-3 degrees, 40% at
// 6-10), and the 10 pin becomes the most common single-pin leave, as with real right-handed pocket hits. Restitution
// barely matters (as USBC found with real balls), and it can't be set in this build anyway.
// So, while it's on: every lane pin collider's material (Collider.material, a per-collider copy) gets the chosen
// friction (static and dynamic), and the ball uses continuous collision. Off, outside Practice or on a new scene, the
// materials get their own values back. It also counts first balls (from a full rack) with it on and off, and logs each.
static const float kPinFricDefault = 0.25f;
static const int kPPMax = 96;
static Ref sPPMat[kPPMax];                    // the pin colliders' materials (per-collider copies)
static float sPPSf0[kPPMax], sPPDf0[kPPMax];   // their own frictions
static int sPPCount = 0, sPPSets = 0, sPPFails = 0;
static bool sPPApplied = false;
static Il2CppClass *sPPMatClass = nullptr, *sPPColClass = nullptr;
static const MethodInfo *sPPGetDf = nullptr, *sPPSetDf = nullptr, *sPPGetSf = nullptr, *sPPSetSf = nullptr, *sPPColMat = nullptr;
static bool sPPTried = false, sThrowSeen = false;
static bool sRateOn = false;                   // (the double physics rate, below)
struct PPStats { int balls, strikes, pins, leaves[10]; };
static PPStats sPPStats[3];                   // [0] the game's own pins, [1] realistic, [2] realistic + double rate

static float PinFriction() { return gBF.pinFric >= 0.05f && gBF.pinFric <= 1.0f ? gBF.pinFric : kPinFricDefault; }

static bool PPResolve() {
    if (sPPTried) return sPPSetDf && sPPSetSf && sPPColMat;
    sPPTried = true;
    ExtraResolve();
    sPPMatClass = FindClass("UnityEngine", "PhysicsMaterial");
    sPPColClass = FindClass("UnityEngine", "Collider");
    sPPGetDf = XM(sPPMatClass, "get_dynamicFriction", 0);
    sPPSetDf = XM(sPPMatClass, "set_dynamicFriction", 1, "System.Single");
    sPPGetSf = XM(sPPMatClass, "get_staticFriction", 0);
    sPPSetSf = XM(sPPMatClass, "set_staticFriction", 1, "System.Single");
    sPPColMat = XM(sPPColClass, "get_material", 0);
    return sPPSetDf && sPPSetSf && sPPColMat;
}

static float PPFloat(const MethodInfo *m, void *obj, float def) {
    bool ok = false;
    Il2CppObject *b = m ? Invoke(m, obj, nullptr, &ok) : nullptr;
    return ok && b ? *(float *)Unbox(b) : def;
}

// Every collider under the lane pins (the pin GameObject and two levels of children), and its material.
static void PPCollect(void *tr, int depth) {
    if (!tr || depth > 2 || sPPCount >= kPPMax) return;
    bool ok = false;
    void *go = (void *)Invoke(X.comp_go, tr, nullptr, &ok);
    void *ga[] = { TypeOf(sPPColClass) };
    void *col = ok && go ? (void *)Invoke(X.go_getComponent, go, ga, &ok) : nullptr;
    if (ok && Alive(col)) {
        void *mat = (void *)Invoke(sPPColMat, col, nullptr, &ok);   // (makes this collider's own copy, once)
        if (ok && Alive(mat)) {
            sPPSf0[sPPCount] = PPFloat(sPPGetSf, mat, 0.3f);
            sPPDf0[sPPCount] = PPFloat(sPPGetDf, mat, 0.5f);
            sPPMat[sPPCount++].set(mat);
        }
    }
    int n = InvokeInt(X.tr_childCount, tr, nullptr, 0);
    for (int i = 0; i < n && i < 16; i++) {
        void *ia[] = { &i };
        void *ch = (void *)Invoke(X.tr_child, tr, ia, &ok);
        if (ok && ch) PPCollect(ch, depth + 1);
    }
}

static void PPSet(int i, float sf, float df) {
    void *mat = sPPMat[i].get();
    if (!Alive(mat)) { sPPFails++; return; }
    void *a[] = { &sf }, *b[] = { &df };
    bool ok1 = false, ok2 = false;
    Invoke(sPPSetSf, mat, a, &ok1);
    Invoke(sPPSetDf, mat, b, &ok2);
    if (ok1 && ok2) sPPSets++; else sPPFails++;
}

static void PPRestore() {
    for (int i = 0; i < sPPCount; i++) PPSet(i, sPPSf0[i], sPPDf0[i]);
    sPPApplied = false;
}

static void PinPhysTick() {
    if (!sSettled) return;
    // (a shot replay is still Practice: gBFStatus.offline is false while LOC_REPLAYER is up)
    bool want = gBF.pinPhys && (gBFStatus.offline || (sMode == MODE_FUN && sLoc == LOC_REPLAYER));
    // a real throw (the ball leaves the hand faster than 1 m/s; picking a ball up passes LOC_THROWING too)
    if (sLoc == LOC_THROWING && !sThrowSeen && N.RB_getVel) {
        void *rb = BallBody();
        bool ok = false;
        Il2CppObject *bv = rb ? Invoke(N.RB_getVel, rb, nullptr, &ok) : nullptr;
        if (ok && bv) { BFVec3 v = *(BFVec3 *)Unbox(bv); if (v.x * v.x + v.y * v.y + v.z * v.z > 1.0f) sThrowSeen = true; }
    }
    if (sFrame % 30 != 0) return;
    if (!want && !sPPApplied) return;
    if (!PPResolve() || !X.tr_child || !X.go_getComponent || !X.comp_go) return;
    if (sPPCount && !Alive(sPPMat[0].get())) { sPPCount = 0; sPPApplied = false; }   // a new scene: find them again
    if (!want) { PPRestore(); BFLog(PP_LOG, "off: the game's own pin friction is back"); return; }
    if (!sPPCount) {
        Il2CppArray *k = TurnKegels();
        for (size_t i = 0; k && i < Len(k); i++) {
            void *go = Elem(k, i);
            bool ok = false;
            void *tr = Alive(go) ? (void *)Invoke(N.GO_getTransform, go, nullptr, &ok) : nullptr;
            if (ok && tr) PPCollect(tr, 0);
        }
        if (!sPPCount) return;
        char msg[160];
        snprintf(msg, sizeof(msg), "found %d pin colliders (their own friction %.2f static / %.2f dynamic)", sPPCount, sPPSf0[0], sPPDf0[0]);
        BFLog(PP_LOG, (const char *)msg);
    }
    float f = PinFriction();
    for (int i = 0; i < sPPCount; i++) {                 // set, and put back if anything changed it
        void *mat = sPPMat[i].get();
        if (!Alive(mat)) continue;
        if (!sPPApplied || fabsf(PPFloat(sPPGetDf, mat, f) - f) > 0.001f || fabsf(PPFloat(sPPGetSf, mat, f) - f) > 0.001f) PPSet(i, f, f);
    }
    if (!sPPApplied) {
        char msg[96];
        snprintf(msg, sizeof(msg), "on: pin friction %.2f", f);
        BFLog(PP_LOG, (const char *)msg);
    }
    sPPApplied = true;
}

static void PinPhysRack(int knocked, bool fullRack, unsigned leave) {
    if (!sThrowSeen) return;                            // a rack without a throw (picking up a ball, spare mode...)
    sThrowSeen = false;
    char pins[40] = "";
    for (int i = 0; i < 10; i++)
        if (leave >> i & 1) snprintf(pins + strlen(pins), sizeof(pins) - strlen(pins), "%s%d", pins[0] ? "-" : "", i + 1);
    char msg[160];
    if (!fullRack) {
        snprintf(msg, sizeof(msg), "second ball: %d down%s%s (%s)", knocked, pins[0] ? ", left " : ", all down", pins, sPPApplied ? "realistic" : "game's own");
        BFLog(PP_LOG, (const char *)msg);
        return;
    }
    PPStats &s = sPPStats[sPPApplied ? (sRateOn ? 2 : 1) : 0];
    s.balls++;
    s.pins += knocked;
    if (knocked >= 10) s.strikes++;
    else if (knocked == 9) for (int i = 0; i < 10; i++) if (leave >> i & 1) s.leaves[i]++;
    snprintf(msg, sizeof(msg), "first ball: %s%s%s (%s, friction %.2f)", knocked >= 10 ? "strike" : "", knocked >= 10 ? "" : "left ", knocked >= 10 ? "" : pins,
             sPPApplied ? (sRateOn ? "realistic, double rate" : "realistic") : "game's own", sPPApplied ? PinFriction() : sPPCount ? sPPDf0[0] : 0.5f);
    BFLog(PP_LOG, (const char *)msg);
}

// ---- double physics rate (Practice, experimental) ----
// What the engine has (Unity 6000.0.67f1, read from libunity.so and UnityFramework; the same on both): the setter of
// Time.fixedDeltaTime is stripped (managed and native), but the native getter survives. It calls the TimeManager getter
// (its first BL) and reads the step as a rational: count (int64, +0x50) x denominator (u32, +0x5c) / numerator (u32,
// +0x58) = 1058399 / 141120000 s = 7.5 ms. The TimeManager's own sync (a virtual method) copies those 16 bytes to +0x70
// and stores 1/step as a float at +0x84. BowlingPlus writes the halved count to +0x50 and +0x70 and 2/step to +0x84 (what
// that sync would write for half the step), only after checking all of them hold exactly the expected values and the
// game's own getter agrees; it reads the getter back afterwards, and puts the old values back when it's off, outside
// Practice, or on a new scene. Safety: during every throw the ball's measured speed (how far it moves per real second)
// is compared with its own velocity; if physics runs fast or slow (ratio outside 0.8..1.25) twice in a row, it undoes the
// change and stays off until the game restarts. In the PhysX model (PIN_PHYSICS_STUDY.md): Bowlscore 37.5% -> 40.5%,
// pocket throws 37% -> 46% strikes, entry angle matters more, the 10 pin still the most common single-pin leave.
typedef void *(*BFTimeMgrFn)();
static uint8_t *sTM = nullptr;
static bool sRateTried = false, sRateBroken = false, sRateLastThrow = false;
static int64_t sRateCount0 = 0;
static float sRateInv0 = 0;
static const char *sRateWhy = "off";
static const MethodInfo *sRateGetDt = nullptr;
static int sRateChecks = 0, sRateBad = 0;
static float sRateRatio = 0, sRateRatioOff = 0;

static float RateManagedStep() {
    if (!sRateGetDt) sRateGetDt = XM(FindClass("UnityEngine", "Time"), "get_fixedDeltaTime", 0);
    return PPFloat(sRateGetDt, nullptr, -1.0f);
}

// In the native step getter's code: the first BL (the TimeManager getter), if the step is read right after it
// (ldr x?, [x0, #0x50] and ldr w?, [x0, #0x58]) as in both builds. Returns the BL's target, or null and why.
static const uint8_t *RateFindGetter(const uint32_t *code, const char **why) {
    for (int i = 0; i < 6; i++) {
        uint32_t ins = code[i];
        if ((ins & 0xFC000000u) != 0x94000000u) continue;
        bool r50 = false, r58 = false;
        for (int k = i + 1; k < i + 6; k++) {
            uint32_t c = code[k];
            if ((c & 0xFFFFFFE0u) == (0xF9400000u | (0x50u / 8) << 10)) r50 = true;
            if ((c & 0xFFFFFFE0u) == (0xB9400000u | (0x58u / 4) << 10)) r58 = true;
        }
        if (!r50 || !r58) { *why = "the step getter isn't the expected code"; return nullptr; }
        int32_t imm = (int32_t)(ins << 6) >> 6;
        return (const uint8_t *)(code + i) + (intptr_t)imm * 4;
    }
    *why = "the step getter has no call";
    return nullptr;
}

static uint8_t *RateTimeManager() {
    if (sTM || sRateTried) return sTM;
    sRateTried = true;
    typedef void *(*ResolveFn)(const char *);
    ResolveFn resolve = (ResolveFn)Api("il2cpp_resolve_icall");
    if (!resolve) { sRateWhy = "no il2cpp_resolve_icall"; return nullptr; }
    const uint32_t *code = (const uint32_t *)resolve("UnityEngine.Time::get_fixedDeltaTime");
    if (!code) code = (const uint32_t *)resolve("UnityEngine.Time::get_fixedDeltaTime()");
    if (!code) { sRateWhy = "no native step getter"; return nullptr; }
    const uint8_t *fn = RateFindGetter(code, &sRateWhy);
    if (!fn) return nullptr;
    sTM = (uint8_t *)((BFTimeMgrFn)fn)();
    if (!sTM) sRateWhy = "no TimeManager";
    return sTM;
}

static double RateStepAt(const uint8_t *tm, int off) {           // a rational at tm+off, in seconds
    int64_t count = *(const int64_t *)(tm + off);
    uint32_t num = *(const uint32_t *)(tm + off + 8), den = *(const uint32_t *)(tm + off + 12);
    return num ? (double)count * den / num : 0.0;
}

static bool RateApply(bool on) {
    uint8_t *tm = RateTimeManager();
    if (!tm) return false;
    if (on) {
        double dt = RateStepAt(tm, 0x50);
        float managed = RateManagedStep();
        bool same = *(int64_t *)(tm + 0x70) == *(int64_t *)(tm + 0x50) && *(uint64_t *)(tm + 0x78) == *(uint64_t *)(tm + 0x58);
        float inv = *(float *)(tm + 0x84);
        if (dt < 0.002 || dt > 0.05 || fabs(managed - dt) > 1e-5 || !same || fabsf(inv - (float)(1.0 / dt)) > 0.01f * (float)(1.0 / dt)) {
            sRateWhy = "the engine's step isn't laid out as expected (left alone)";
            return false;
        }
        sRateCount0 = *(int64_t *)(tm + 0x50);
        sRateInv0 = inv;
        int64_t half = (sRateCount0 + 1) / 2;
        *(int64_t *)(tm + 0x50) = half;
        *(int64_t *)(tm + 0x70) = half;
        *(float *)(tm + 0x84) = (float)(1.0 / RateStepAt(tm, 0x50));
        float now = RateManagedStep();
        if (fabs(now - dt / 2) > 1e-5) {                            // the game doesn't see it: put it back
            *(int64_t *)(tm + 0x50) = sRateCount0;
            *(int64_t *)(tm + 0x70) = sRateCount0;
            *(float *)(tm + 0x84) = sRateInv0;
            sRateWhy = "the game didn't see the new step (put back)";
            return false;
        }
        sRateOn = true;
        sRateWhy = "on";
        char msg[120];
        snprintf(msg, sizeof(msg), "double physics rate on: step %.3f ms -> %.3f ms", dt * 1000, now * 1000.0);
        BFLog(PP_LOG, (const char *)msg);
        return true;
    }
    if (!sRateOn) return true;
    *(int64_t *)(tm + 0x50) = sRateCount0;
    *(int64_t *)(tm + 0x70) = sRateCount0;
    *(float *)(tm + 0x84) = sRateInv0;
    sRateOn = false;
    if (!sRateBroken) sRateWhy = "off";
    char msg[96];
    snprintf(msg, sizeof(msg), "double physics rate off: step back to %.3f ms", RateManagedStep() * 1000.0);
    BFLog(PP_LOG, (const char *)msg);
    return true;
}

// During a throw: does the ball move as fast as its velocity says? (physics time = real time)
static double sSpdT0 = -1, sSpdY0 = 0, sSpdVSum = 0;
static int sSpdN = 0;
static bool sSpdDone = false;
static void RateSpeedCheck() {
    if (sLoc != LOC_THROWING) { sSpdT0 = -1; sSpdDone = false; return; }
    sRateLastThrow = sRateOn;                       // the rate this throw (and so its replay) is recorded at
    if (sSpdDone || !N.RB_getVel) return;
    void *rb = BallBody();
    bool ok = false;
    Il2CppObject *bv = rb ? Invoke(N.RB_getVel, rb, nullptr, &ok) : nullptr;
    if (!ok || !bv) return;
    BFVec3 v = *(BFVec3 *)Unbox(bv);
    void *tr = Invoke(N.Comp_getTransform, rb, nullptr, &ok);
    Il2CppObject *pb = ok && tr && X.tr_getPos ? Invoke(X.tr_getPos, tr, nullptr, &ok) : nullptr;
    if (!ok || !pb) return;
    float y = ((BFVec3 *)Unbox(pb))->y;
    double t = PinNow();
    if (v.y < 3.0f || y > 16.5f) { if (sSpdT0 >= 0 && v.y < 3.0f) sSpdT0 = -1; return; }   // rolling down the lane, before the pins
    if (sSpdT0 < 0) { sSpdT0 = t; sSpdY0 = y; sSpdVSum = 0; sSpdN = 0; return; }
    sSpdVSum += v.y;
    sSpdN++;
    if (t - sSpdT0 < 0.3 || sSpdN < 8) return;
    float ratio = (float)(((y - sSpdY0) / (t - sSpdT0)) / (sSpdVSum / sSpdN));
    sSpdDone = true;
    char msg[120];
    if (!sRateOn) { sRateRatioOff = ratio; snprintf(msg, sizeof(msg), "speed check (normal rate): ball moved at %.2f x its velocity", ratio); BFLog(PP_LOG, (const char *)msg); return; }
    sRateRatio = ratio;
    sRateChecks++;
    bool bad = ratio < 0.8f || ratio > 1.25f;
    sRateBad = bad ? sRateBad + 1 : 0;
    snprintf(msg, sizeof(msg), "speed check (double rate): ball moved at %.2f x its velocity%s", ratio, bad ? " - out of range" : "");
    BFLog(PP_LOG, (const char *)msg);
    if (sRateBad >= 2) {
        sRateBroken = true;
        sRateWhy = "switched itself off: physics ran at the wrong speed";
        RateApply(false);
    }
}

static void RateTick() {
    if (!sSettled) return;
    RateSpeedCheck();
    if (sFrame % 30 != 0) return;
    // A replay plays one recorded frame per physics step, so it runs at the rate its throw was recorded at (1.7.1 put
    // the normal rate back on the replay screen, which played double-rate throws at half speed).
    bool replay = sMode == MODE_FUN && sLoc == LOC_REPLAYER;
    bool want = gBF.pinPhys && gBF.pinRate2x && !sRateBroken && (replay ? sRateLastThrow : gBFStatus.offline);
    if (want && !sRateOn) RateApply(true);
    else if (!want && sRateOn) RateApply(false);
    else if (sRateOn && sTM && *(int64_t *)(sTM + 0x50) != (sRateCount0 + 1) / 2) {   // something put the step back
        sRateOn = false;
        BFLog(PP_LOG, "the engine's step changed by itself: double rate will be applied again");
    }
}

static void RateDebug(char *buf, size_t size) {
    snprintf(buf, size, "physics rate: want=%d on=%d step=%.3f ms (game %.3f) | %s | speed checks %d, last %.2f (normal rate %.2f)",
             gBF.pinRate2x ? 1 : 0, sRateOn ? 1 : 0, RateManagedStep() * 1000.0f, sRateCount0 ? sRateCount0 * 1000.0 / 141120000.0 : 7.5,
             sRateWhy, sRateChecks, sRateRatio, sRateRatioOff);
}

// ---- the game's own settings, for the game modes ----
// What the game has (1.907): GameParams.GetSetting(type) reads the current mode's GameSettings (GameParams._allSettings,
// a Dictionary<GameModes, GameSettings>; mode NONE (4) reads another mode). GameParams.SetSetting saves to disk
// (GameSettings.SaveState); GameSettings.SetSetting on the mode's own object changes memory only. SettingType:
// G_SOUNDS 0, OFF_CROSSOVER 1, CHANGE_OIL 2, CHANGE_LANE 3, SHOW_OIL_PATTERN 4, LIFT_BUMPERS 5, FRAME_COUNT 6 ...
// OilMapGenerator.IsActive = GetSetting(SHOW_OIL_PATTERN) > 0 && player level > 2, and ReDrawOil only shows the oil when
// it's active, so with that setting at 0 in memory the game never puts the oil on the lane.
static const MethodInfo *sGsGet = nullptr, *sGsSetMem = nullptr, *sGsSetSaved = nullptr, *sGsItem = nullptr;
static Il2CppClass *sGsParams = nullptr;
static void *GameSettingsNow() {
    if (!sGsParams) sGsParams = FindClass("", "GameParams");
    if (!sGsParams) return nullptr;
    void *dict = nullptr;
    static FieldInfo *all = nullptr;
    if (!all) all = StaticField(sGsParams, "_allSettings");
    if (!all) return nullptr;
    StaticRead(all, &dict);
    if (!dict) return nullptr;
    if (!sGsItem) sGsItem = XM(ClassOf(dict), "get_Item", 1);
    int mode = sMode;
    void *a[] = { &mode };
    bool ok = false;
    void *gs = sGsItem ? (void *)Invoke(sGsItem, dict, a, &ok) : nullptr;
    if (!ok || !gs) return nullptr;
    if (!sGsGet) {
        sGsGet = XM(ClassOf(gs), "GetSetting", 1);
        sGsSetMem = XM(ClassOf(gs), "SetSetting", 2);
        sGsSetSaved = XM(sGsParams, "SetSetting", 2);
    }
    return gs;
}
static int GameSettingGet(int type) {
    void *gs = GameSettingsNow();
    void *a[] = { &type };
    return gs && sGsGet ? InvokeInt(sGsGet, gs, a, -1) : -1;
}
static bool GameSettingSet(int type, int value, bool save) {   // save: through GameParams (as the game's own menu does)
    void *gs = GameSettingsNow();
    if (!gs) return false;
    void *a[] = { &type, &value };
    bool ok = false;
    if (save && sGsSetSaved) Invoke(sGsSetSaved, nullptr, a, &ok);
    else if (sGsSetMem) Invoke(sGsSetMem, gs, a, &ok);
    return ok;
}

#define NT_LOG "9-pin no-tap: %s"
// ---- game mode: 9-pin no-tap (Practice) ----
// What the game has (1.907, RunPsycsTest's update, constructor values): after a throw, once every pin is slower than
// sleepVelocity (0.1 m/s) or asleep, it sets `end` and counts `_curTimeout` down from `_timeout` (1.5 s), then calls
// endThrought, which reads each pin's position and up direction, builds the standing list and scores the throw
// (InfoScreenManager.ProcessThrow); `_globalTimeout` (3.5 s) ends it anyway if the pins never settle.
// No-tap: on a ball from a full rack, as soon as `end` is set with exactly one pin standing, BowlingPlus pushes that pin
// back toward the pit (it slides and tips over) well inside the 1.5 s, so the game counts 10 and scores its own strike.
// If it's somehow still up with 0.4 s to go, it's put under the deck, where the game keeps the pins that are down.
static int sNtEndOff = -2, sNtCurOff = -2;
static bool sNtThrow = false, sNtFull = false, sNtHandled = false;
static int sNtPin = -1, sNtKnocks = 0, sNtFallbacks = 0;

static bool NtPinStanding(int i, void *go) {
    float lean = TurnPinLean(i);
    if (lean < 0 || lean > 20.0f) return false;
    bool ok = false;
    void *tr = (void *)Invoke(N.GO_getTransform, go, nullptr, &ok);
    Il2CppObject *pb = ok && tr && X.tr_getPos ? Invoke(X.tr_getPos, tr, nullptr, &ok) : nullptr;
    if (!ok || !pb) return false;
    BFVec3 p = *(BFVec3 *)Unbox(pb);
    return p.z > -0.05f && p.z < 0.1f && p.y < 19.2f && p.y > 17.5f;   // upright, on the deck
}

static void NoTapTick() {
    bool want = gBF.noTap9 && gBFStatus.offline && sSettled && sTurnKeep;
    if (!want) { sNtThrow = false; return; }
    void *rpt = sRPT.get();
    if (!rpt || !N.RB_setVel) return;
    if (sNtEndOff == -2) {
        char tn[64];
        Il2CppClass *k = ClassOf(rpt);
        sNtEndOff = FieldTypeName(k, "end", tn, sizeof(tn)) && !strcmp(tn, "System.Boolean") ? FieldOffset(k, "end") : -1;
        sNtCurOff = FieldTypeName(k, "_curTimeout", tn, sizeof(tn)) && !strcmp(tn, "System.Single") ? FieldOffset(k, "_curTimeout") : -1;
    }
    if (sNtEndOff < 0 || sNtCurOff < 0) return;
    if (sLoc == LOC_THROWING && !sNtThrow) {             // a throw: was the rack before it full?
        bool full = sTurnCount == 10;
        for (int i = 0; i < sTurnCount; i++) if (!sTurnHave[i] || !sTurnWasUp[i]) full = false;
        sNtThrow = true;
        sNtFull = full;
        sNtHandled = false;
        sNtPin = -1;
    }
    if (sLoc == LOC_START_POS || sLoc == LOC_BALL_RETURNER) sNtThrow = false;
    if (!sNtThrow || !sNtFull || !At<bool>(rpt, sNtEndOff)) return;
    Il2CppArray *kegs = TurnKegels();
    if (!kegs || Len(kegs) != 10) return;
    if (!sNtHandled) {
        sNtHandled = true;
        int standing = 0, which = -1;
        for (int i = 0; i < 10; i++) {
            void *go = Elem(kegs, i);
            if (Alive(go) && NtPinStanding(i, go)) { standing++; which = i; }
        }
        if (standing != 1) return;
        void *rb = GetComp(Elem(kegs, which), N.tRigidbody);
        if (!rb) return;
        BFVec3 push = { 0.0f, 1.3f, 0.0f };             // toward the pit: friction at the base tips it over
        void *a[] = { &push };
        Invoke(N.RB_setVel, rb, a);
        sNtPin = which;
        sNtKnocks++;
        char msg[96];
        snprintf(msg, sizeof(msg), "9 on the first ball: the %d pin goes over for the strike", which + 1);
        BFLog(NT_LOG, (const char *)msg);
        return;
    }
    if (sNtPin < 0) return;
    float left = At<float>(rpt, sNtCurOff);
    void *go = Elem(kegs, sNtPin);
    if (left < 0.4f && Alive(go) && NtPinStanding(sNtPin, go)) {   // still up: put it where the game keeps down pins
        bool ok = false;
        void *tr = (void *)Invoke(N.GO_getTransform, go, nullptr, &ok);
        Il2CppObject *pb = ok && tr && X.tr_getPos ? Invoke(X.tr_getPos, tr, nullptr, &ok) : nullptr;
        static const MethodInfo *setPos = XM(X.Transform, "set_position", 1, "UnityEngine.Vector3");
        if (ok && pb && setPos) {
            BFVec3 p = *(BFVec3 *)Unbox(pb);
            p.z = -200.0f;
            void *a[] = { &p };
            Invoke(setPos, tr, a);
            sNtFallbacks++;
            BFLog(NT_LOG, "the pin stayed up: put it down");
        }
        sNtPin = -1;
    }
}

static void NoTapDebug(char *buf, size_t size) {
    snprintf(buf, size, "9-pin no-tap: on=%d fields=%d/%d knocked over=%d put down=%d", gBF.noTap9 ? 1 : 0, sNtEndOff, sNtCurOff, sNtKnocks, sNtFallbacks);
}

static void PinPhysStatus(char *buf, size_t size) {
    char part[3][120];
    for (int k = 0; k < 3; k++) {
        const PPStats &s = sPPStats[k];
        if (!s.balls) snprintf(part[k], sizeof(part[k]), "no first balls yet");
        else snprintf(part[k], sizeof(part[k]), "%d first ball%s, %d strike%s (%.0f%%), %.1f pins", s.balls, s.balls == 1 ? "" : "s",
                      s.strikes, s.strikes == 1 ? "" : "s", 100.0 * s.strikes / s.balls, (double)s.pins / s.balls);
    }
    snprintf(buf, size, "Realistic: %s\nRealistic + double rate: %s\nGame's own: %s%s%s", part[1], part[2], part[0],
             sRateBroken ? "\nDouble rate: " : "", sRateBroken ? sRateWhy : "");
}

static void PinPhysDebug(char *buf, size_t size) {
    int topPin = -1, topN = 0;
    for (int i = 0; i < 10; i++) if (sPPStats[1].leaves[i] > topN) { topN = sPPStats[1].leaves[i]; topPin = i + 1; }
    snprintf(buf, size, "pin physics: on=%d friction=%.2f (own %.2f/%.2f) colliders=%d applied=%d sets=%d fails=%d | first balls realistic %d/%d strikes %d pins (top leave %d x%d) | own %d/%d strikes %d pins",
             gBF.pinPhys ? 1 : 0, PinFriction(), sPPCount ? sPPSf0[0] : -1.0f, sPPCount ? sPPDf0[0] : -1.0f, sPPCount, sPPApplied ? 1 : 0, sPPSets, sPPFails,
             sPPStats[1].strikes, sPPStats[1].balls, sPPStats[1].pins, topPin, topN, sPPStats[0].strikes, sPPStats[0].balls, sPPStats[0].pins);
    size_t used = strlen(buf);
    if (used + 1 < size) snprintf(buf + used, size - used, " | double rate %d/%d strikes %d pins", sPPStats[2].strikes, sPPStats[2].balls, sPPStats[2].pins);
}

Str BFPinPhysStatus(void) {                       // for the menu
    char buf[300];
    PinPhysStatus(buf, sizeof(buf));
    return Str(buf);
}

bool BFGamePinToFile(const Str &path) {           // the game's current pin picture (for the pin library), Unity's thread
    std::vector<uint8_t> png;
    if (!sSettled || !GamePinPNG(png)) return false;
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(png.data(), 1, png.size(), f) == png.size();
    fclose(f);
    return ok;
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
        Str nm = NameOf(b);
        if (StrHas(nm, "fulltextures")) {
            void *t = LoadPearlFrom(b);
            if (t) return t;
        }
    }
    if (N.AB_LoadFromFile && N.AB_Unload) {                    // 2) open it, copy the texture, close it
        // iOS: <app>/Data/Raw/AssetBundles/...; Android: the same folder is assets/AssetBundles inside the APK,
        // which Unity reads through Application.streamingAssetsPath ("jar:file://<apk>!/assets")
        Str root = N.App_streaming ? Text(Invoke(N.App_streaming, nullptr, nullptr)) : Str();
        Str path = root + "/AssetBundles/balls/fulltextures";
        void *a[] = { NewString(path.c_str()) };
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
        if (NameOf(t) == kPearlTexName) return t;
    }
    return nullptr;
}

static void *PearlTexture() {
    void *t = sPearlTex ? Target(sPearlTex) : nullptr;
    if (Alive(t)) return t;
    if (sPearlTex) { Release(sPearlTex); sPearlTex = 0; }
    double now = BFNow();
    if (sPearlTries >= 5 || now < sPearlNextTry) return nullptr;
    sPearlTries++;
    sPearlNextTry = now + 4;
    t = LoadPearlTexture();
    if (t) { DontUnload(t); sPearlTex = Keep(t); BFLog("Match Up Pearl sheet loaded (last resort)"); }
    return t;
}

static void *BPTexture() {
    void *t = sBPTex ? Target(sBPTex) : nullptr;
    if (Alive(t)) return t;
    if (sBPTex) { Release(sBPTex); sBPTex = 0; }
    if (sBPTries >= 3 || !N.Texture2D || !N.T2D_ctor || !N.LoadImage || !N.Byte) return nullptr;
    sBPTries++;
    std::vector<uint8_t> png = BFMakeMatchUpBPTexturePNG(512);
    if (png.empty()) return nullptr;
    Il2CppObject *tex = NewObject(N.Texture2D);
    if (!tex) return nullptr;
    int w = 2, h = 2;
    bool ok = false;
    void *ca[] = { &w, &h };
    Invoke(N.T2D_ctor, tex, ca, &ok);
    if (!ok) return nullptr;
    Il2CppArray *bytes = NewArray(N.Byte, png.size());
    if (!bytes) return nullptr;
    memcpy(Data(bytes), png.data(), png.size());
    void *la[] = { tex, bytes };
    if (!InvokeBool(N.LoadImage, nullptr, la, false)) return nullptr;
    DontUnload(tex);
    sBPTex = Keep(tex);
    BFLog("Match Up BP look-alike skin created (last resort)");
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
    BFLog("tutorial: cleared the stage SkipTutorial left behind");
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
            BFLog("skipping the tutorial (the game's own SkipTutorial)");
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
static Ref sLoadingWnd;   // (sCoreLoop is declared before UpdateState, which uses it)
static double sGdprWaitSince = 0, sConnectingSince = 0;
static int sUnstuckCount = 0;

static bool GOActive(void *go) {
    return go && N.GO_activeInHierarchy && InvokeBool(N.GO_activeInHierarchy, go, nullptr, false);
}

static bool SectionActive(void *lw, int off) { return off >= 0 && GOActive(At<void *>(lw, off)); }

static void StuckTick() {
    if (!gBF.unstick || sFrame % 30 != 0) return;
    double now = BFNow();

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
            BFLog("loading: privacy SDK silent for 20 s, carrying on (like with no internet)");
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
    BFLog("loading: stuck connecting for 30 s, showing the game's offline button");
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
static double sSpinnerSince = 0;
static int sSpinnerCount = 0;

static void SpinnerTick() {
    if (sFrame % 30 != 0 || !sSettled || !N.Processing || !N.Proc_Hide || N.proc_count < 0) return;
    void *p = ReadStaticObj(sProcInst, N.Processing, "_instance");
    sSpinnerCount = (p && Alive(p)) ? At<int>(p, N.proc_count) : 0;
    if (!gBF.unstick || sSpinnerCount <= 0) { sSpinnerSince = 0; return; }
    double now = BFNow();
    if (!sSpinnerSince) { sSpinnerSince = now; return; }
    if (now - sSpinnerSince < 30) return;
    int was = sSpinnerCount;
    for (int i = 0; i < 10 && At<int>(p, N.proc_count) > 0; i++) Invoke(N.Proc_Hide, p, nullptr);
    sSpinnerSince = 0;
    sUnstuckCount++;
    BFLog("spinner: no server answer for 30 s, hid it so you can try again (count was %d)", was);
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
static double sStartTime = 0;   // first engine tick (about when the game starts)
static Str sLastDiag, sLastWindows;
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
static Str SStr(Il2CppClass *k, FieldInfo *&f, const char *name) {
    void *o = ReadStaticObj(f, k, name);
    return o ? Text(o) : Str("-");
}

static Str QueueStrings(void *q) {   // System.Collections.Generic.Queue<string>, read by field names
    if (!q) return "-";
    Il2CppClass *k = ClassOf(q);
    int arrOff = FieldOffset(k, "_array"), headOff = FieldOffset(k, "_head"), sizeOff = FieldOffset(k, "_size");
    if (arrOff < 0 || headOff < 0 || sizeOff < 0) return "?";
    Il2CppArray *arr = At<Il2CppArray *>(q, arrOff);
    int head = At<int>(q, headOff), size = At<int>(q, sizeOff);
    size_t len = Len(arr);
    if (!arr || size < 0 || len == 0 || (size_t)size > len || head < 0 || (size_t)head >= len) return "?";
    std::vector<Str> out;
    for (int i = 0; i < size && i < 8; i++) {
        void *s = ((void **)Data(arr))[((size_t)head + i) % len];
        out.push_back(s ? Text(s) : Str("?"));
    }
    return "[" + StrJoin(out, " ") + "]";
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
        BFLog("game logger: level was %d, set to 0 (All) for 2 minutes", sLoggerOrig);
    }
    int all = 0;
    if (BFNow() - sStartTime < 120) {
        StaticWrite(sLgLevel, &all);
        if (global && levelOff >= 0) At<int>(global, levelOff) = 0;
    } else {
        StaticWrite(sLgLevel, &sLoggerOrig);
        sLoggerRestored = true;
        BFLog("game logger: back to level %d", sLoggerOrig);
    }
}

static void DiagTick() {
    if (sFrame % 30 != 0) return;
    GameLoggerTick();
    // where startup is
    Str core = "-";
    bool pastInit = sSettled;
    void *mcl = sCoreLoop.get();
    if (!mcl && N.tMonoCoreLoop && sFrame % 60 == 0) { mcl = FirstAlive(FindAll(N.tMonoCoreLoop)); sCoreLoop.set(mcl); }
    void *sm = (mcl && N.mcl_sm >= 0) ? At<void *>(mcl, N.mcl_sm) : nullptr;
    void *state = (sm && N.sm_state >= 0) ? At<void *>(sm, N.sm_state) : nullptr;
    if (state) {
        core = ClassName(ClassOf(state));
        if (ClassOf(state) == N.InitState && N.is_gdpr >= 0) core += Fmt("(gdprDone=%d)", At<bool>(state, N.is_gdpr) ? 1 : 0);
        else pastInit = true;
    }
    void *lw = sLoadingWnd.get();
    if (!lw && N.tLoadingWindow && sFrame % 60 == 0) { lw = FirstAlive(FindAll(N.tLoadingWindow)); sLoadingWnd.set(lw); }
    Str load = "-";
    if (lw) {
        pastInit = true;
        load = Fmt("%c%c%c%c%c",
                SectionActive(lw, N.lw_noConn) ? 'N' : 'n', SectionActive(lw, N.lw_conn) ? 'C' : 'c', SectionActive(lw, N.lw_login) ? 'L' : 'l',
                SectionActive(lw, N.lw_online) ? 'O' : 'o', SectionActive(lw, N.lw_offline) ? 'F' : 'f');
    }
    Str conn = "(not started)";
    if (pastInit && N.Connector) {
        static const char *names[] = { "OFFLINE", "MASTER_CONNECTING", "MASTER_CONNECTED", "GAME_CONNECTING", "GAME_CONNECTED" };
        int st = SInt(N.Connector, sCsState, "_state");
        void *peer = ReadStaticObj(sCsPeer, N.Connector, "peer");
        Str server = "-";
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
            if (addr) { void *sa = Invoke(addr, peer, nullptr); server = sa ? Text(sa) : Str("?"); }
            if (tp) proto = InvokeInt(tp, peer, nullptr, -1);
        }
        Str relays = QueueStrings(ReadStaticObj(sCsRelays, N.Connector, "_relays"));
        conn = Fmt("%s(%d) connecting=%d reconnect=%d discByServer=%d noReconnect=%d server=%s proto=%d ipv6=%d redirect=%s relays=%s api=%s/%s",
                st >= 0 && st <= 4 ? names[st] : "?", st, SBool(N.Connector, sCsConnecting, "_isConnecting"),
                SBool(N.Connector, sCsDoReconnect, "doReconnect"), SBool(N.Connector, sCsDiscByServer, "wasDiscByServer"),
                SBool(N.Connector, sCsWithout, "WithoutReconnect"), server, proto, v6, SStr(N.Connector, sCsRedirect, "redirectTo"), relays,
                SStr(N.Connector, sCsClientApi, "Client_API_Version"), SStr(N.Connector, sCsServerApi, "Server_API_Version"));
        if (N.Reconnect)
            conn += Fmt(" | reconnectMgr state=%d count=%d end=%d", SInt(N.Reconnect, sRmState, "reconnectState"),
                    SInt(N.Reconnect, sRmCount, "reconnectCount"), SInt(N.Reconnect, sRmEnd, "reconnectSequenceEnd"));
    }
    int reach = N.App_reach ? InvokeInt(N.App_reach, nullptr, nullptr, -1) : -1;   // 0 none, 1 cellular, 2 wifi
    Str diag = Fmt("core=%s | loading=%s | conn=%s | reach=%d | spinner=%d", core, load, conn, reach, sSpinnerCount);
    if (diag != sLastDiag) {
        sLastDiag = diag;
        BFLogEvent("game", diag);
    }
    // which game windows are open (every 2 s)
    if (sFrame % 120 == 0 && N.tFloatWnd) {
        std::vector<Str> names;
        Il2CppArray *all = FindAll(N.tFloatWnd);
        for (size_t i = 0; i < Len(all); i++) {
            void *w = Elem(all, i);
            if (Alive(w) && GOActive(GameObjectOf(w))) names.push_back(ClassName(ClassOf(w)));
        }
        std::sort(names.begin(), names.end());
        Str list = names.empty() ? Str("(none)") : StrJoin(names, ", ");
        if (list != sLastWindows) {
            sLastWindows = list;
            BFLogEvent("ui", "windows open: " + list);
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



static void SetTargetFps(int v) {
    void *a[] = { &v };
    Invoke(N.App_setFps, nullptr, a);
}

// While a BowlingPlus panel (the menu, the oil library, the editor...) is open, the lane behind it is dimmed
// and not being played, but the game would keep drawing it at 60 or 120 FPS. That load shares the phone's GPU
// with the panel's own scrolling, which then stutters. So while a panel is open the game is capped to the
// 30 FPS it already uses for its own menus, and put back the moment the panel closes.
static int sOverlayRestoreFps = -1;     // the rate to go back to; -1 = not capped right now
static bool sOverlayWas = false;

static void OverlayFpsTick() {
    if (!sSettled || !N.App_setFps || !N.App_getFps) return;
    bool ov = BFOverlayVisible();
    bool change = ov != sOverlayWas;
    if (!change && !(ov && sFrame % 30 == 0)) return;   // only act on a change, or re-check once in a while
    sOverlayWas = ov;
    int cur = InvokeInt(N.App_getFps, nullptr, nullptr, -1);
    if (ov) {
        if (cur > 30) {
            if (sOverlayRestoreFps < 0) sOverlayRestoreFps = cur;
            SetTargetFps(30);
            if (change) BFLog("panel open: game capped to 30 FPS (was %d)", cur);
        }
    } else if (sOverlayRestoreFps > 0) {
        int back = (gBF.fps120 && sFpsApplied) ? 120 : sOverlayRestoreFps;
        SetTargetFps(back);
        BFLog("panel closed: game back to %d FPS", back);
        sOverlayRestoreFps = -1;
    }
}

static void FpsTick() {
    OverlayFpsTick();
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
            BFLog("120 FPS: frame rate table menu %d / game %d -> 120", sOrigMenuFps, sOrigGameFps);
        }
        if (sOverlayRestoreFps < 0 && InvokeInt(N.App_getFps, nullptr, nullptr, 120) != 120) SetTargetFps(120);   // (not while a panel has it capped)
    } else if (sFpsApplied) {
        int game = sOrigGameFps > 0 ? sOrigGameFps : 60, menu = sOrigMenuFps > 0 ? sOrigMenuFps : 30;
        StaticWrite(sGameFpsF, &game);
        StaticWrite(sMenuFpsF, &menu);
        SetTargetFps(game);   // the game sets its own value again at the next screen change
        sFpsApplied = false;
        if (sOverlayRestoreFps >= 0) sOverlayRestoreFps = game;   // a panel has it capped: closing it restores the normal rate, not 120
        BFLog("120 FPS: off, frame rate table back to %d / %d", menu, game);
    }
}

Str BFFpsLine(void) {
    // iOS also needed CADisableMinimumFrameDurationOnPhone in Info.plist; Android has no such switch, but the
    // screen must offer a 120 Hz mode (BP.java asks the window for the fastest one while this is on)
    long screenMax = BFScreenMaxHz();
    if (screenMax > 0 && screenMax < 119) return Fmt("This screen tops out at %ld Hz.", screenMax);
    if (!gBF.fps120) return "Off: the game's normal 30 FPS menus / 60 FPS gameplay.";
    if (!sFpsApplied) return "On: applies once the lane has loaded.";
    int cur = N.App_getFps ? InvokeInt(N.App_getFps, nullptr, nullptr, -1) : -1;
    return Fmt("On: game asks for %d FPS (screen max %ld Hz).", cur, screenMax);
}

// ---------------------------------------------------------------------------
// Arsenal search
// The Arsenal list draws from ArsenalBallManager._ballsData (a List of your balls).
// We keep only the matching balls in that list and tell the scroll view to reload.
// InitScrollData() puts the full list back.
// ---------------------------------------------------------------------------
static Str sQuery, sAppliedQuery;   // "" = none (nil on iOS)
static Ref sArsenal;
static void *sFilteredList = nullptr;
static int sFilteredVersion = 0, sShown = -1, sTotal = -1;
static bool sArsenalDirty = false;

void BFSetArsenalQuery(const Str &q) {
    sQuery = StrTrim(q);
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
    if (sQuery.empty() && sAppliedQuery.empty()) { sArsenalDirty = false; return; }
    void *ars = sArsenal.get();
    if (!ars && (sArsenalDirty || sFrame % 30 == 0)) {
        ars = FirstAlive(FindAll(N.tArsenal));
        sArsenal.set(ars);
    }
    sArsenalDirty = false;
    if (!ars) { sFilteredList = nullptr; sAppliedQuery.clear(); sShown = sTotal = -1; return; }

    if (sQuery.empty()) {                             // search cleared: put the full list back
        if (N.Ars_InitScrollData) { Invoke(N.Ars_InitScrollData, ars, nullptr); ResetArsenalScroll(ars); }
        sAppliedQuery.clear(); sFilteredList = nullptr; sShown = sTotal = -1;
        return;
    }
    void *list = At<void *>(ars, N.ars_ballsData);
    ListView lv;
    if (!ReadList(list, lv)) return;
    bool rebuilt = list != sFilteredList || lv.version != sFilteredVersion;   // the game refreshed the list
    if (!rebuilt && sQuery == sAppliedQuery) return;
    if (!rebuilt) {                                    // new search on an already-filtered list
        if (!N.Ars_InitScrollData) return;
        Invoke(N.Ars_InitScrollData, ars, nullptr);
        list = At<void *>(ars, N.ars_ballsData);
        if (!ReadList(list, lv)) return;
    }
    std::vector<Str> words;
    for (const Str &w : StrSplitWS(sQuery)) {
        Str s = Squash(w);
        if (!s.empty()) words.push_back(s);
    }
    int keep = 0;
    for (int i = 0; i < lv.size; i++) {
        void *it = lv.items[i];
        if (!it) continue;
        Str name = Squash(ItemName(it));
        bool match = true;
        for (const Str &w : words) if (!StrHas(name, w)) { match = false; break; }
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
    BFLog("arsenal search '%s': %d of %d balls", sQuery, sShown, sTotal);
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

// The mirror fix needs the oil texture on Repeat, and this game only has Unity's one wrap setting for both
// directions (set_wrapModeU / V are stripped). Along the lane that wraps the far edge onto the foul-line row:
// the GPU blends the two over the last half row, which drew a thin line of heavy oil, in the oil color,
// behind the pins. The shader's lane coordinate is y / _SizeY and the lane surface ends exactly at 1, so
// _SizeY is made a hair longer: the far edge then stops inside the last row, short of the wrap. For a
// 240-row map that's 0.25% (the drawing ends about an inch short at 40 ft). Looks only: the oil you bowl
// on is the game's grid, which this doesn't touch. The game's own value is kept per material and put back
// when the mirror fix is off.
static struct { void *mat; float base; } sSizeY[16];
static int sIdSizeY = 0, sSizeYRows = 0;
static float sSizeYScale = 1.0f;

static void LaneLengthFix(void *mat, void *tex) {
    if (!sIdSizeY && N.Sh_propToId) { void *a[] = { NewString("_SizeY") }; sIdSizeY = InvokeInt(N.Sh_propToId, nullptr, a, 0); }
    void *ha[] = { NewString("_SizeY") };
    if (!sIdSizeY || !InvokeBool(N.M_hasProp, mat, ha, false)) return;
    void *ga[] = { &sIdSizeY };
    bool ok = false;
    Il2CppObject *boxed = Invoke(N.M_getFloatI, mat, ga, &ok);
    if (!ok || !boxed) return;
    float v = *(float *)Unbox(boxed);
    int slot = -1, empty = -1;
    for (int j = 0; j < 16; j++) {
        if (sSizeY[j].mat == mat) { slot = j; break; }
        if (!sSizeY[j].mat && empty < 0) empty = j;
    }
    if (slot < 0) {                                // first time we see this material: its value is the game's
        static int sNextSlot = 0;
        if (v == 0) return;
        slot = empty >= 0 ? empty : (sNextSlot++ % 16);   // full: reuse the oldest entry
        sSizeY[slot].mat = mat; sSizeY[slot].base = v;
    }
    float base = sSizeY[slot].base;
    int rows = (N.T_getH && Alive(tex)) ? InvokeInt(N.T_getH, tex, nullptr, 0) : 0;
    if (rows < 16) rows = 240;                     // the game's maps are 240 rows (4 per foot over 60 ft)
    float scale = gBF.oilMirrorFix ? rows / (rows - 0.6f) : 1.0f;   // last sample lands 0.1 row inside the edge
    sSizeYRows = rows; sSizeYScale = scale;
    float want = base * scale;
    if (fabsf(v - want) > fabsf(base) * 1e-5f) {
        void *sa[] = { NewString("_SizeY"), &want };
        Invoke(N.M_setFloatS, mat, sa);
    }
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
        LaneLengthFix(mat, sLaneTex[i]);
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
static Str sInvisNote;

static int RandomUnlockedOil(int avoid) {         // returns OIL_SELECTED value (1 + list index), or -1
    if (!sPmData) sPmData = StaticField(N.PracticeMgr, "_practiceData");
    void *pd = nullptr;
    if (sPmData) StaticRead(sPmData, &pd);
    static int oilDataOff = -2;
    if (pd && oilDataOff == -2) oilDataOff = FieldOffset(ClassOf(pd), "oil_data");
    Il2CppArray *arr = (pd && oilDataOff >= 0) ? At<Il2CppArray *>(pd, oilDataOff) : nullptr;
    ListView oils;
    if (!arr || !Len(arr) || !OilList(oils)) { sInvisNote = "couldn't read your unlocked patterns"; return -1; }
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
    if (picks.empty()) { sInvisNote = "no other unlocked patterns"; return -1; }
    sInvisNote = Fmt("picks from your %lu unlocked patterns", (unsigned long)picks.size() + 1);
    return picks[arc4random_uniform((uint32_t)picks.size())];
}

// The oil used to flash on the lane for a moment after picking a ball: the game's SelectBall redraws the oil and shows it
// (OilMapGenerator.ReDrawOil), and this tick hid it again a frame later. Now, while invisible oil is on, the game's own
// "show oil pattern" setting is 0 in memory (not saved), so the game never shows it; it's restored (and saved) when
// invisible oil is turned off. gBF.oilShowOrig keeps the original across a crash.
#define INVIS_LOG "invisible oil: %s"
static int sInvisShowOrig = 0;
static void OilInvisibleTick() {
    void *gen = OilGenerator();
    bool want = gBF.oilInvisible && InPracticeOil();
    bool safe = sLoc == LOC_START_POS || sLoc == LOC_BALL_RETURNER || sLoc == LOC_UPPER_SCREEN;
    if (!sInvisShowOrig && gBF.oilShowOrig > 0) sInvisShowOrig = gBF.oilShowOrig;   // (left over from a crash)
    if (want && sSettled && sMode == MODE_FUN) {
        int cur = GameSettingGet(4);                    // SHOW_OIL_PATTERN
        if (cur > 0 && GameSettingSet(4, 0, false)) {
            if (!sInvisShowOrig) sInvisShowOrig = cur;
            if (gBF.oilShowOrig != sInvisShowOrig) { gBF.oilShowOrig = sInvisShowOrig; BFSaveConfig(); }
            bool off = false;
            void *a[] = { &off };
            if (gen && N.OG_showOnLane) Invoke(N.OG_showOnLane, gen, a);   // hide what's on the lane now
            BFLog(INVIS_LOG, "the game's oil display is off while invisible oil is on");
        }
    }
    if (!want && sInvisShowOrig > 0 && safe && sSettled && sMode == MODE_FUN) {
        if (GameSettingSet(4, sInvisShowOrig, true)) {  // back, and saved (as the game's own settings menu does)
            sInvisShowOrig = 0;
            gBF.oilShowOrig = 0;
            BFSaveConfig();
            if (gen && N.OG_reloadRedraw && sInvisOrig <= 0) Invoke(N.OG_reloadRedraw, gen, nullptr);   // show it again
            BFLog(INVIS_LOG, "the game's oil display is back");
        }
    }
    if (!want) {
        if (sInvisOrig > 0 && safe && gen && N.OG_reloadRedraw) {   // put the pattern you picked back
            SetOilSelected(sInvisOrig);
            Invoke(N.OG_reloadRedraw, gen, nullptr);
            BFLog("invisible oil: off, back to your pattern");
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
    BFLog("invisible oil: random unlocked pattern for this game");
}

// ---- 4) custom patterns through the game's Kegel engine ----
static Json sCustomPattern;                       // set by the pattern library; null = off
static Str sCustomAppliedId;                      // "" = nothing applied
static ObjRef sCustomTarget;
static int sCustomTargetIdx = -1;               // its list position (the picture cache's key)
static Json sLastKegel;                           // what the engine computed for the custom pattern (oil report)
static Str sOilReportSource;
static int sBuiltinPatched = 0;                   // how many of the game's own patterns are drawn from their Kegel files right now
static int sLastKegelBase = -1;
static std::vector<float> sCustomBackup;
static Str sCustomNote;

static void *TemplateAsset(int index) {           // a real pattern file, for the machine settings
    void *d = OilDescAt(index);
    if (!d) d = OilDescAt(0);
    return (d && sOdSrcAsset >= 0) ? At<void *>(d, sOdSrcAsset) : nullptr;
}

static bool KegelSteps(void *pattern, const Json *fwd, const Json *rev) {
    static int fOff = -2, rOff = -2;
    if (fOff == -2) { fOff = FieldOffset(ClassOf(pattern), "mForward"); rOff = FieldOffset(ClassOf(pattern), "mReverse"); }
    void *lists[2] = { fOff >= 0 ? At<void *>(pattern, fOff) : nullptr, rOff >= 0 ? At<void *>(pattern, rOff) : nullptr };
    const Json *steps[2] = { fwd, rev };
    for (int d = 0; d < 2; d++) {
        void *list = lists[d];
        if (!list) return false;
        const MethodInfo *clear = FindMethod(ClassOf(list), "Clear", 0), *add = FindMethod(ClassOf(list), "Add", 5);
        if (!clear || !add) return false;
        Invoke(clear, list, nullptr);
        for (const Json &s : Items(steps[d])) {
            if (s.size() < 4) continue;
            int start = s[0].i(), stop = s[1].i(), loads = s[2].i(), speed = s[3].i();
            // the engine works out a step's end from loads x speed; zero-load (travel only) steps use this
            float ef = s.size() > 4 ? s[4].f() : 0;
            void *a[] = { &start, &stop, &loads, &speed, &ef };
            Invoke(add, list, a);
        }
    }
    return true;
}

static Json KegelReadSteps(void *list) {          // [[start, stop, loads, speed, endFeet], ...]
    ListView lv;
    Json out = Json::Arr();
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
        out.push(JNums({ (double)At<int>(ls, sO), (double)At<int>(ls, eO), (double)At<int>(ls, lO), (double)At<int>(ls, pO),
                         ok && f ? (double)*(float *)Unbox(f) : 0.0 }));
    }
    return out;
}

// Runs the game's Kegel engine. With steps == nil, returns the template pattern's own steps.
// drop = the sheet's "Reverse Brush Drop" in feet (Pattern.mTravel, 0xC4). Graph3DReverse only lays a
// reverse step's oil if that step ends short of it, so a custom pattern must not inherit the template's
// value. 0 = keep the template's.
static Json KegelRun(int templateIndex, const Json *fwd, const Json *rev, int drop) {
    if (!N.D_drawFromFile || !N.D_graphF || !N.D_graphR || !N.Drawer) return Json();
    void *asset = TemplateAsset(templateIndex);
    if (!asset) return Json();
    void *a[] = { asset };
    bool ok = false;
    Invoke(N.D_drawFromFile, nullptr, a, &ok);
    if (!ok) return Json();
    void *pattern = ReadStaticObj(sDrPattern, N.Drawer, "mPattern");
    if (!pattern) return Json();
    static int travOff = -2;
    if (travOff == -2) travOff = FieldOffset(ClassOf(pattern), "mTravel");
    int templateDrop = travOff >= 0 ? At<int>(pattern, travOff) : -1;
    if ((fwd || rev) && drop > 0 && travOff >= 0) At<int>(pattern, travOff) = drop;
    int usedDrop = travOff >= 0 ? At<int>(pattern, travOff) : -1;
    if (fwd || rev) {
        if (!KegelSteps(pattern, fwd, rev)) return Json();
        OilGrid u;
        if (!GridOf(ReadStaticObj(sDrUnits, N.Drawer, "Units"), u)) return Json();
        memset(u.data, 0, sizeof(float) * u.w * u.h);
        Invoke(N.D_graphF, nullptr, nullptr, &ok);
        if (!ok) return Json();
        Invoke(N.D_graphR, nullptr, nullptr, &ok);
        if (!ok) return Json();
    }
    OilGrid u;
    if (!GridOf(ReadStaticObj(sDrUnits, N.Drawer, "Units"), u)) return Json();
    int w = 0, h = 0;
    if (!sGpMapW) sGpMapW = StaticField(N.GameParams, "OIL_MAP_WIDTH");
    if (!sGpMapH) sGpMapH = StaticField(N.GameParams, "OIL_MAP_LENGTH");
    if (sGpMapW) StaticRead(sGpMapW, &w);
    if (sGpMapH) StaticRead(sGpMapH, &h);
    if (w <= 0 || h <= 0 || w > 1024 || h > 16384) return Json();
    std::vector<float> grid((size_t)w * h);
    float *g = grid.data(), maxv = 0, sum = 0;
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
    Json r = Json::Obj();
    r.set("w", w); r.set("h", h); r.set("grid", Json::FloatsOf(std::move(grid))); r.set("max", maxv); r.set("sum", sum);
    r.set("fwd", KegelReadSteps(fl)); r.set("rev", KegelReadSteps(rl)); r.set("drop", usedDrop); r.set("tdrop", templateDrop);
    return r;
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
static void KegelEnds(const Json *fwdIn, const Json *revIn, bool exact, std::vector<KStep> &fwd, std::vector<KStep> &rev) {
    float prev = 0;
    bool first = true;
    for (int d = 0; d < 2; d++) {
        for (const Json &a : Items(d == 0 ? fwdIn : revIn)) {
            if (a.size() < 4) continue;
            KStep k = { a[0].i(), a[1].i(), a[2].i(), a[3].i(), a.size() > 4 ? a[4].f() : 0,
                        a.size() > 5 && a[5].f() > 0 ? a[5].f() : 50.f };
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

static std::vector<KStep> KegelParseSteps(const Json *in) {
    std::vector<KStep> out;
    for (const Json &a : Items(in)) {
        if (a.size() < 4) continue;
        out.push_back({ a[0].i(), a[1].i(), a[2].i(), a[3].i(), a.size() > 4 ? a[4].f() : 0,
                        a.size() > 5 && a[5].f() > 0 ? a[5].f() : 50.f });
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

static Json KegelDrawPattern(const Json *fwdIn, const Json *revIn, int drop, bool exact, bool custom, int feet = 0, bool precise = false) {
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
    std::vector<float> grid((size_t)w * h);
    float *g = grid.data(), maxv = 0, sum = 0;
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
    Json fo = Json::Arr(), ro = Json::Arr();
    for (const KStep &k : fwd) fo.push(JNums({ (double)k.start, (double)k.stop, (double)k.loads, (double)k.speed, k.end, k.ul }));
    for (const KStep &k : rev) ro.push(JNums({ (double)k.start, (double)k.stop, (double)k.loads, (double)k.speed, k.end, k.ul }));
    Json r = Json::Obj();
    r.set("w", w); r.set("h", h); r.set("grid", Json::FloatsOf(std::move(grid))); r.set("max", maxv); r.set("sum", sum);
    r.set("fwd", fo); r.set("rev", ro); r.set("drop", drop > 0 ? drop : 60); r.set("tdrop", drop); r.set("feet", feet);
    return r;
}

// Engine self-check for the oil report: draw one of the game's own patterns with our copy (its steps and
// brush drop as the game reads them) and compare with the game's own drawing.
static Str KegelSelfCheck(int index) {
    Json game = KegelRun(index, nullptr, nullptr, 0);
    if (game.isNull()) return "self-check: couldn't read the pattern";
    Json ours = KegelDrawPattern(&game["fwd"], &game["rev"], game["tdrop"].i(), true, false);
    const Json &a = game["grid"], &b = ours["grid"];
    if (!a.fl || !b.fl || a.fl->size() != b.fl->size()) return "self-check: size mismatch";
    const float *x = a.fl->data(), *y = b.fl->data();
    size_t n = a.fl->size(), bad = 0;
    float worst = 0;
    for (size_t i = 0; i < n; i++) { float d = fabsf(x[i] - y[i]); if (d > 0.05f) bad++; worst = fmaxf(worst, d); }
    return Fmt("self-check vs game's own drawing of pattern %d: max diff %.3f, %zu of %zu cells differ", index + 1, worst, bad, n);
}

static void RestoreCustom() {
    void *d = sCustomTarget.get();
    OilGrid src, live;
    if (d && sOdSource >= 0 && GridOf(At<void *>(d, sOdSource), src) && (size_t)src.w * src.h == sCustomBackup.size()) {
        memcpy(src.data, sCustomBackup.data(), sizeof(float) * sCustomBackup.size());
        if (sOdMatrix >= 0 && GridOf(At<void *>(d, sOdMatrix), live) && live.w == src.w && live.h == src.h)
            memcpy(live.data, src.data, sizeof(float) * sCustomBackup.size());
        ReloadOilIndex(sCustomTargetIdx);         // redraw THAT pattern's picture, not whichever is current
        BFLog("custom oil: the original pattern is back");
    }
    sCustomTarget.set(nullptr);
    sCustomTargetIdx = -1;
    sCustomBackup.clear();
    sCustomAppliedId.clear();
}

static void OilCustomTick() {
    bool want = !sCustomPattern.isNull() && InPracticeOil() && !gBF.oilInvisible;
    void *cur = CurrentOilDesc();
    bool safe = sLoc == LOC_START_POS || sLoc == LOC_BALL_RETURNER || sLoc == LOC_UPPER_SCREEN;
    Str pid = sCustomPattern["id"].str();
    if (!sCustomAppliedId.empty() && (!want || cur != sCustomTarget.get() || pid != sCustomAppliedId)) {
        if (!want || safe) RestoreCustom();
    }
    if (!want || !sCustomAppliedId.empty() || !cur || !safe || sOdSource < 0 || sOdMatrix < 0) return;
    OilGrid src, live;
    if (!GridOf(At<void *>(cur, sOdSource), src) || !GridOf(At<void *>(cur, sOdMatrix), live) || src.w != live.w || src.h != live.h) return;
    // Build from the pattern's own starting template, never from the pattern it replaces: the template's
    // machine settings would leak in and the same custom pattern would look different on each host.
    ListView ol;
    int base = sCustomPattern["base"].i();
    if (!OilList(ol) || base < 0 || base >= ol.size) base = 0;
    Json r = KegelDrawPattern(&sCustomPattern["fwd"], &sCustomPattern["rev"], sCustomPattern["drop"].i(),
                              sCustomPattern["exact"].truthy(), true,
                              sCustomPattern["feet"].i(), sCustomPattern["precise"].truthy());
    sLastKegel = r;
    sLastKegelBase = base;
    const Json &grid = r["grid"];
    if (r["w"].i() != src.w || r["h"].i() != src.h || !grid.fl || grid.fl->size() != (size_t)src.w * src.h) {
        sCustomNote = "couldn't build the pattern";
        sCustomAppliedId = pid.empty() ? Str("?") : pid;      // don't retry every tick
        return;
    }
    sCustomBackup.assign(src.data, src.data + (size_t)src.w * src.h);
    memcpy(src.data, grid.fl->data(), sizeof(float) * grid.fl->size());    // a new game copies this clean grid back, so it stays
    memcpy(live.data, grid.fl->data(), sizeof(float) * grid.fl->size());
    sCustomTarget.set(cur);
    sCustomTargetIdx = OilSelected() - 1;
    sCustomAppliedId = pid.empty() ? Str("?") : pid;
    sCustomNote.clear();
    ReloadOilIndex(sCustomTargetIdx);
    BFLog("custom oil: \"%s\" is on the lane", sCustomPattern["name"].str());
}

// ---- 4b) the game's own patterns, drawn like real life (Practice) ----
// The game builds its 48 patterns with its simplified Kegel engine (whole-foot rows, reverse oil doubling,
// no film on boards the oil head never crossed, left and right mirrored). Each pattern still carries its
// real Kegel file (OilDescription._source, a TextAsset), so in Practice the lane's oil grids are replaced by
// the Kegel-accurate drawing of that file: exact distances on the lane's quarter-foot rows, microliters,
// the brushed film, and Kegel's left on the bowler's left. All 48 are swapped at once (so the pattern
// carousel shows them right away), the originals are kept, and everything is put back the moment you're not
// in Practice (online matches, tournaments, the tutorial always get the game's own oil).
static std::map<int, Json> sBuiltinSpec;      // null = its file couldn't be read
static struct BuiltinState { void *desc = nullptr; bool applied = false; std::vector<float> backup; } sBI[64];
static uint64_t sBuiltinRedraw = 0;               // patterns whose lane picture still needs redrawing
static int sBuiltinFails = 0;

static const Json *BuiltinSpec(int idx, void *desc) {
    if (idx < 0 || idx >= 64 || !desc) return nullptr;
    auto found = sBuiltinSpec.find(idx);
    if (found != sBuiltinSpec.end()) return found->second.isNull() ? nullptr : &found->second;
    OilDescOffsets(desc);
    Json spec;
    void *ta = sOdSrcAsset >= 0 ? At<void *>(desc, sOdSrcAsset) : nullptr;
    if (Alive(ta) && N.TA_getText) {
        Str text = Text(Invoke(N.TA_getText, ta, nullptr));
        if (!text.empty()) {
            std::vector<std::string> lines = StrSplit(text, "\n");
            KegelFile f = KegelParseLines(lines);
            if (f.ok) {
                Json fw = Json::Arr(), rv = Json::Arr();
                for (const KegelFileStep &k : f.fwd) fw.push(JNums({ (double)k.start, (double)k.stop, (double)k.loads, (double)k.speed, k.end, (double)f.ul }));
                for (const KegelFileStep &k : f.rev) rv.push(JNums({ (double)k.start, (double)k.stop, (double)k.loads, (double)k.speed, k.end, (double)f.ul }));
                Str nm = sOdName >= 0 ? Text(At<void *>(desc, sOdName)) : Str();
                spec = Json::Obj();
                spec.set("fwd", fw); spec.set("rev", rv); spec.set("drop", f.drop); spec.set("feet", f.feet); spec.set("ul", f.ul);
                spec.set("name", nm.empty() ? Str("pattern") : nm);
            }
        }
    }
    Json &slot = sBuiltinSpec[idx];
    slot = spec;
    if (spec.isNull()) { sBuiltinFails++; BFLog("built-in oil pattern %d: couldn't read its Kegel file, it keeps the game's drawing", idx + 1); return nullptr; }
    return &slot;
}

Json BFOilBuiltinSpec(int idx) {                  // for the editor's "Start from"
    void *d = OilDescAt(idx);
    const Json *s = d ? BuiltinSpec(idx, d) : nullptr;
    return s ? *s : Json();
}

static void BuiltinRestoreAll() {
    if (!sBuiltinPatched) { for (int i = 0; i < 64; i++) sBI[i] = BuiltinState(); return; }
    if (!sCustomAppliedId.empty()) RestoreCustom();   // a custom pattern may sit on top of one of them: back to the patched first
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
    BFLog("the game's own oil patterns are back (not in Practice)");
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
        if (!sCustomAppliedId.empty() && i == sCustomTargetIdx) continue;     // a custom pattern is on this one right now
        OilDescOffsets(d);
        if (sOdSource < 0 || sOdMatrix < 0) return;
        const Json *spec = BuiltinSpec(i, d);
        if (!spec) continue;
        OilGrid src, live;
        if (!GridOf(At<void *>(d, sOdSource), src) || !GridOf(At<void *>(d, sOdMatrix), live) || src.w != live.w || src.h != live.h) continue;
        Json r = KegelDrawPattern(&(*spec)["fwd"], &(*spec)["rev"], (*spec)["drop"].i(), true, true, (*spec)["feet"].i(), true);
        const Json &grid = r["grid"];
        if (r["w"].i() != src.w || r["h"].i() != src.h || !grid.fl || grid.fl->size() != (size_t)src.w * src.h) continue;
        sBI[i].backup.assign(src.data, src.data + (size_t)src.w * src.h);
        memcpy(src.data, grid.fl->data(), sizeof(float) * grid.fl->size());   // a new game copies this clean grid back, so it stays
        memcpy(live.data, grid.fl->data(), sizeof(float) * grid.fl->size());
        sBI[i].desc = d;
        sBI[i].applied = true;
        sBuiltinRedraw |= 1ull << i;
        added++;
    }
    int total = 0;
    for (int i = 0; i < 64; i++) total += sBI[i].applied;
    sBuiltinPatched = total;
    if (added) BFLog("Practice: %d of the game's oil patterns are now drawn from their Kegel files (%d couldn't be read)", total, sBuiltinFails);
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
        BFLog("oil thickness colors %s", sThickApplied ? "on" : "off");
    }
    ListView lv;
    if (sThickRedrawNext < 0 || !OilList(lv)) return;
    for (int n = 0; n < 6 && sThickRedrawNext < lv.size; n++) ThickRedrawIndex(sThickRedrawNext++);
    if (sThickRedrawNext >= lv.size) sThickRedrawNext = -1;
}

// "Copy debug info" oil report: what the engine computed for the custom pattern next to what is on the
// lane right now, sampled every 5 ft on an outside (3), mid (10) and center (20) board.
static Str OilReport(bool live) {
    Str s;
    Json r = sLastKegel;
    sOilReportSource = !sCustomAppliedId.empty() ? "custom pattern, Kegel-accurate model" : "";
    if (sCustomAppliedId.empty() && live && sBuiltinPatched) {   // the game's own pattern, drawn from its Kegel file
        int idx = OilSelected() - 1;
        void *dd = CurrentOilDesc();
        const Json *spec = (dd && idx >= 0) ? BuiltinSpec(idx, dd) : nullptr;
        if (spec) {
            r = KegelDrawPattern(&(*spec)["fwd"], &(*spec)["rev"], (*spec)["drop"].i(), true, true, (*spec)["feet"].i(), true);
            sOilReportSource = Fmt("game pattern \"%s\" from its Kegel file, Kegel-accurate model (%d of 48 patterns patched)", (*spec)["name"].str(), sBuiltinPatched);
        }
    }
    int w = r["w"].i(), h = r["h"].i();
    const float *eg = r["grid"].fl ? r["grid"].fl->data() : nullptr;
    OilGrid lane = {};
    void *d = live ? CurrentOilDesc() : nullptr;
    if (d) { OilDescOffsets(d); if (sOdMatrix >= 0) GridOf(At<void *>(d, sOdMatrix), lane); }
    if (r.isNull() && !lane.data) return "";
    int lw = lane.data ? lane.w : w, lh = lane.data ? lane.h : h;
    s += Fmt("oil report: map %dx%d (%.2f rows/ft) | custom template #%d brush drop used %s (template %s) | %s | lane = pattern %d\n",
        lw, lh, lh / 60.0, sLastKegelBase, r.has("drop") ? r["drop"].str() : Str("-"), r.has("tdrop") ? r["tdrop"].str() : Str("-"),
        sOilReportSource, live ? OilSelected() : -1);
    std::vector<Str> fe, re;
    for (const Json &st : r["fwd"].items()) fe.push_back(Fmt("%.1f", st[4].f()));
    for (const Json &st : r["rev"].items()) re.push_back(Fmt("%.1f", st[4].f()));
    if (!r.isNull()) s += Fmt("  engine step ends: fwd %s | rev %s\n", StrJoin(fe, " "), StrJoin(re, " "));
    if (live) s += Fmt("  %s\n", KegelSelfCheck(OilSelected() - 1));
    s += "  ft: engine c3/c10/c20 | lane c3/c10/c20 (lane columns, 1 = bowler's right)\n";
    int b[3] = { 3, 10, 20 };
    for (int ft = 0; ft <= 45; ft += 5) {
        s += Fmt("  %2d:", ft);
        for (int k = 0; k < 3; k++) {
            int x = b[k] - 1, y = h > 0 ? (int)((ft + 0.5) * h / 60.0) : 0;
            s += Fmt(" %5.1f", (eg && x < w && y < h) ? eg[(size_t)x * h + y] : -1.f);
        }
        s += " |";
        for (int k = 0; k < 3; k++) {
            int x = b[k] - 1, y = (int)((ft + 0.5) * lh / 60.0);
            s += Fmt(" %5.1f", (lane.data && x < lane.w && y < lane.h) ? lane.data[(size_t)x * lane.h + y] : -1.f);
        }
        s += "\n";
    }
    return s;
}

void BFOilApplyHue(void) {                         // called by the color picker: the next frame shows it
    if (gBF.oilHue >= 0) sHueShown = gBF.oilHue;
}

#define LANE_LOG_GAVEUP "other lane: the game keeps moving you back, stopped trying (turn the switch off and on to retry)"
#define LANE_LOG_MOVE "other lane: asked the game to move from lane %d to %d (now %d)"
// ---- bowl on the other lane (experimental, Practice) ----
// The game has two lanes: RoadChanger.shiftBy = { 0, -1.837 } and the static RoadChanger.currentLane says
// which one you're on (device, 1.6.3: lane 1). Dragging your shoes at the ball rack moves you over, but the
// game snaps back when you let go. This asks the game's own RoadChanger.shiftToLaneNumber(int) for the
// other lane while you're at the ball rack. "Home" is the lane the game picks by itself; turning the
// setting off goes back there. If the game keeps moving you back, it stops after 5 tries and says so.
static Il2CppClass *sRoadClass = nullptr;
static const MethodInfo *sRoadShift = nullptr;
static FieldInfo *sRoadCur = nullptr;
static bool sRoadLooked = false, sLaneWas = false, sLaneGaveUp = false;
static bool sLaneMoved = false;   // BowlingPlus put you on the lane you're on now
static Ref sRoad;
static int sLaneHome = -1, sLaneTarget = -1, sLaneTries = 0, sLaneReverts = 0, sLaneLastTry = -100000;

static int LaneNow() {
    if (!sRoadCur) return -1;
    int v = -1;
    StaticRead(sRoadCur, &v);
    return v;
}

// 1.6.5: switched off. On a device (1.6.4) the game moved you straight back every time (5 of 5) and the
// other lane has no pins, oil or lights in Practice: it isn't set up there. Making it playable needs the
// game's code read (what RoadChanger.toActivate0/1, getCurrentLaneGame and SwitchLaneDrag.OnEndDrag do),
// which needs a disassembler. The code stays for that; the menu switch is gone, and a saved "on" from
// 1.6.4 does nothing.
static const bool kLaneSwitchEnabled = false;

static void LaneTick() {
    if (!kLaneSwitchEnabled) return;
    if (sFrame % 30 != 0 || !sSettled) return;
    if (!sRoadLooked) {
        sRoadLooked = true;
        sRoadClass = FindClass("", "RoadChanger");
        if (sRoadClass) {
            sRoadShift = FindMethod(sRoadClass, "shiftToLaneNumber", 1, "System.Int32");
            char tn[64];
            if (FieldTypeName(sRoadClass, "currentLane", tn, sizeof(tn)) && !strcmp(tn, "System.Int32") && FieldOffset(sRoadClass, "currentLane") < 0x10)
                sRoadCur = StaticField(sRoadClass, "currentLane");
        }
    }
    if (!sRoadClass || !sRoadShift || !sRoadCur) return;
    bool want = gBF.laneOther && InPractice();
    if (want != sLaneWas) { sLaneWas = want; sLaneTries = sLaneReverts = 0; sLaneGaveUp = false; sLaneTarget = -1; }
    int cur = LaneNow();
    if (cur < 0 || cur > 1) return;
    if (!sLaneMoved) sLaneHome = cur;      // until BowlingPlus moves you, your lane is the game's own choice
    if (!InPractice()) return;
    int target = gBF.laneOther ? 1 - sLaneHome : sLaneHome;
    if (cur == target) { if (!gBF.laneOther) sLaneMoved = false; return; }   // back home: the game's again
    if (sLaneGaveUp) return;
    if (sLaneTarget == target && sFrame - sLaneLastTry < 240) return;       // give the last move time
    if (sLaneTarget == target && sLaneTries > 0) sLaneReverts++;            // we moved, and it's back
    if (sLaneReverts >= 5) {
        if (!sLaneGaveUp) { sLaneGaveUp = true; BFLog(LANE_LOG_GAVEUP); }
        return;
    }
    if (sLoc != LOC_BALL_RETURNER && sLoc != LOC_UPPER_SCREEN) return;      // only between shots, at the rack
    void *road = sRoad.get();
    if (!road) { road = FirstAlive(FindAll(TypeOf(sRoadClass))); sRoad.set(road); }
    if (!road) return;
    void *a[] = { &target };
    Invoke(sRoadShift, road, a);
    sLaneTarget = target; sLaneTries++; sLaneLastTry = sFrame;
    if (gBF.laneOther) sLaneMoved = true;
    BFLog(LANE_LOG_MOVE, cur, target, LaneNow());
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

Json BFOilBuiltins(void) {
    Json out = Json::Arr();
    ListView lv;
    if (!OilList(lv)) return out;
    for (int i = 0; i < lv.size; i++) {
        void *d = lv.items[i];
        if (!d) continue;
        OilDescOffsets(d);
        Str nm = sOdName >= 0 ? Text(At<void *>(d, sOdName)) : Str();
        nm = StrStripTags(nm);                     // the game's names can carry rich-text tags: "<size=50>2011 USBC Masters</size>"
        Json e = Json::Obj();
        e.set("index", i);
        e.set("name", nm.empty() ? Fmt("Pattern %d", i + 1) : nm);
        e.set("feet", sOdDist >= 0 ? At<float>(d, sOdDist) : 0);
        e.set("ml", sOdVol >= 0 ? At<float>(d, sOdVol) : 0);
        out.push(e);
    }
    return out;
}

// fwd/rev nil: the game's own pattern at templateIndex (its steps, as the game reads them, and its drop).
// Otherwise: our copy of the engine draws the given steps.
Json BFOilCompute(int templateIndex, const Json *fwd, const Json *rev, int drop, bool exact, int feet, bool precise) {
    try {
        if (!fwd && !rev) {                        // one of the game's own patterns: its real Kegel file (microliters, exact ends)
            Json spec = BFOilBuiltinSpec(templateIndex);
            if (!spec.isNull()) return KegelDrawPattern(&spec["fwd"], &spec["rev"], spec["drop"].i(), true, true, spec["feet"].i(), true);
            return KegelRun(templateIndex, nullptr, nullptr, 0);
        }
        return KegelDrawPattern(fwd, rev, drop, exact, true, feet, precise);
    } catch (...) { return Json(); }
}



void BFOilSetCustom(const Json *pattern) { sCustomPattern = pattern ? *pattern : Json(); }

bool BFPracticeLobbyOpen(void) {
    if (!N.ok || !sSettled || !N.tFloatWnd) return false;
    static Ref lobby;
    static double lastSearch = 0;
    void *w = lobby.get();
    double now = BFNow();
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

Str BFOilStatusLine(void) {
    std::vector<Str> parts;
    if (gBF.oilMirrorFix && sMirrorLanes) parts.push_back("oil drawn on the correct side");
    if (gBF.oilInvisible) parts.push_back("invisible oil " + (sInvisNote.empty() ? Str() : "(" + sInvisNote + ")"));
    if (!sCustomPattern.isNull()) {
        Str nm = sCustomPattern["name"].str();
        parts.push_back(!sCustomAppliedId.empty() && sCustomNote.empty() ? Fmt("custom \"%s\" on the lane", nm)
                        : Fmt("custom \"%s\" %s", nm, sCustomNote.empty() ? Str("(starts in practice)") : sCustomNote));
    }
    return StrJoin(parts, " \u00B7 ");
}

// ---- tap the game's pin layouts to pick pins (Practice) ----
// Holding the ball, the top-right pin layout (MainMenuButtonMan.pinObj) shows which pins stand; in the overhead
// view of the ball return, the little screen under it (the "MonitorCollider" box) does. A tap on either opens
// the pin picker for this shot only.
static Ref sMmbm, sMonCol;
static Str sTapNote = "no tap yet";

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
    const char *what = nullptr;
    float r[4] = {};
    if (sLoc == LOC_START_POS) {                                     // holding the ball: the top-right layout
        void *m = sMmbm.get();
        if (!m && N.tMMBM) { m = FirstAlive(FindAll(N.tMMBM)); sMmbm.set(m); }
        if (m && N.mmbm_pinBack >= 0 && UiRectOnScreen(At<void *>(m, N.mmbm_pinBack), r) && Inside(r, u, v)) what = "top-right pin layout";
        else if (m && N.mmbm_pinObj >= 0 && UiRectOnScreen(At<void *>(m, N.mmbm_pinObj), r) && Inside(r, u, v)) what = "top-right pin layout";
    } else if (sLoc == LOC_BALL_RETURNER || sLoc == LOC_BOTTOM_MONITOR) {   // overhead of the ball return: the screen under it
        void *c = sMonCol.get();
        if (!c && N.tBoxCollider) {
            Il2CppArray *all = FindAll(N.tBoxCollider);
            for (size_t i = 0; i < Len(all) && !c; i++) {
                void *o = Elem(all, i);
                if (Alive(o) && NameOf(o) == "MonitorCollider") c = o;
            }
            sMonCol.set(c);
        }
        if (c && ColliderRectOnScreen(c, r) && Inside(r, u, v)) what = "screen under the ball return";
    }
    sTapNote = Fmt("loc=%d tap u=%.3f v=%.3f rect=%.2f,%.2f-%.2f,%.2f -> %s", sLoc, u, v, r[0], r[1], r[2], r[3], what ? what : "not on a pin layout");
    if (!what) return false;
    BFLog("pin layout tapped (%s): opening the pin picker for this shot", what);
    BFMenuShowPinPickerOneShot(StandingMask());
    return true;
}

// ---------------------------------------------------------------------------
// Main loop + menu text
// ---------------------------------------------------------------------------
void BFEngineTick(void) {
    sFrame++;
    if (sStartTime == 0) sStartTime = BFNow();
    if (gBFSafeMode) return;
    if (!N.ok) {
        static int attempts = 0;
        // hands off for the first 5 seconds while the game boots
        if (sFrame < 300 || sFrame % 30 != 0 || attempts > 200) return;
        if (!Ready()) return;
        attempts++;
        try { N.ok = Resolve(); } catch (...) { N.ok = false; BFLog("error while connecting to the game"); }
        gBFStatus.engineReady = N.ok;
        if (!N.ok) return;
    }
    {
        try {
            UpdateState();
            TutorialTick();
            StuckTick();
            SpinnerTick();
            DiagTick();
            OilTick();
            FpsTick();
            PinTurnTick();
            PinPhysTick();
            RateTick();
            NoTapTick();
            BgTick();
            BannerTopTick();
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
            LaneTick();
        } catch (...) {
            BFLog("C++ exception in tick");
        }
    }
    sPrevLoc = sLoc;
}

Str BFStatusLine(void) {
    if (gBFSafeMode) return "Safe mode: the game didn't finish starting twice in a row, so BowlingPlus paused itself to keep the game working.";
    if (!gBFStatus.engineReady || !sSettled) return "Waiting for the game to finish loading...";
    if (sInTutorial) return "Tutorial: use the Skip tutorial button at the top right";
    if (gBFStatus.offline) return "Practice (offline): everything is active";
    if (gBFStatus.gameMode == MODE_COMPETE) return "Online match: fun stuff is paused";
    if (gBFStatus.gameMode == MODE_TUTORIAL) return "Tutorial: fun stuff is paused";
    return "Menus / online: fun stuff is paused";
}

Str BFBallLine(void) {
    std::vector<Str> lines;
    if (!sBallLine.empty()) lines.push_back(sBallLine);
    if (!gBF.textureFix) {
        lines.push_back("Skin fix: off");
        return StrJoin(lines, "\n");
    }
    switch (sPearlFail) {
        case PF_OK:      lines.push_back("Match Up Pearl: fixed (its burgundy art, from the game's own Pearl/Hybrid skin file)"); break;
        case PF_WAITING: lines.push_back("Match Up Pearl: waiting for the store data..."); break;
        case PF_NO_PEARL:lines.push_back("Match Up Pearl: not in the store list"); break;
        case PF_NO_SHEET:lines.push_back("Match Up Pearl: its skin file isn't in the game's catalog, fixed in your hand only"); break;
        default:         lines.push_back("Match Up Pearl: couldn't change the game data, fixed in your hand only"); break;
    }
    if (sBPFixed) lines.push_back("Match Up BP: fixed (its Black Pearl art; the game pointed it at a file that isn't in the app)");
    else if (sCurSkin == SKIN_BP && !sBPHasSkin && sPearlFail != PF_WAITING)
        lines.push_back("Match Up BP: no working skin found, drawn look-alike in your hand");
    return StrJoin(lines, "\n");
}

Str BFArsenalLine(void) {
    if (sQuery.empty()) return "Type part of a ball name and tap Search, then open the Arsenal.";
    if (sShown >= 0) return Fmt("Showing %d of %d balls for \"%s\"", sShown, sTotal, sQuery);
    return Fmt("Searching for \"%s\" - open the Arsenal", sQuery);
}

static float InvokeFloat(const MethodInfo *m, void *obj, float fallback) {
    bool ok = false;
    Il2CppObject *r = Invoke(m, obj, nullptr, &ok);
    return (ok && r) ? *(float *)Unbox(r) : fallback;
}

static Str PathInfo(int id) {                // "123 Text_X_Y.png" for the debug info
    if (id < 0) return "none";
    Str p = DlcPath(id);
    if (p.empty()) return Fmt("%d (not in catalog)", id);
    return Fmt("%d %s%s", id, StrLastPath(p), Shipped(id) == 0 ? " (NOT in app files)" : "");
}

// ---- the game's two-lane system (read-only, for Copy debug info) ----
// The game still has it: RoadChanger (currentLane, laneNum, oneLane, shiftBy, shiftToLaneNumber(int)),
// SwitchLaneDrag (the drag-your-shoes gesture), DragManager.firstLaneLeft / secondLaneRight and the
// setting SettingType.CHANGE_LANE. Nothing here changes anything: it reports what the game has live, with
// each field's real C# type, so switching lanes can be added on facts instead of guesses.
static void FieldText(void *obj, Il2CppClass *k, const char *name, char *out, size_t n) {
    int off = k ? FieldOffset(k, name) : -1;
    if (off < 0) { snprintf(out, n, " %s=missing", name); return; }
    char tn[96];
    FieldTypeName(k, name, tn, sizeof(tn));
    // An instance field never sits below 0x10 (the object header), so a smaller offset is a static field's
    // offset in the class's static data (1.6.2 read currentLane from the object header: 475208576).
    alignas(8) unsigned char sbuf[16] = { 0 };
    bool isStatic = off < 0x10;
    if (isStatic) { FieldInfo *f = StaticField(k, name); if (!f) { snprintf(out, n, " %s=?", name); return; } StaticRead(f, sbuf); obj = sbuf; off = 0; }
    const char *tag = isStatic ? "(static)" : "";
    if (!obj) { snprintf(out, n, " %s:%s", name, tn[0] ? tn : "?"); return; }
    if (!strcmp(tn, "System.Single[]")) {   // print the array's contents (lane offsets?)
        Il2CppArray *arr = At<Il2CppArray *>(obj, off);
        int len = arr ? (int)Len(arr) : -1, w = snprintf(out, n, " %s%s=[", name, tag);
        for (int j = 0; arr && j < len && j < 6 && w > 0 && (size_t)w < n; j++) w += snprintf(out + w, n - w, j ? ",%.3f" : "%.3f", ((float *)Data(arr))[j]);
        if (w > 0 && (size_t)w < n) snprintf(out + w, n - w, len > 6 ? ",...](%d)" : "]", len);
        return;
    }
    if (!strcmp(tn, "System.Int32")) snprintf(out, n, " %s%s=%d", name, tag, At<int>(obj, off));
    else if (!strcmp(tn, "System.Single")) snprintf(out, n, " %s%s=%.3f", name, tag, At<float>(obj, off));
    else if (!strcmp(tn, "System.Boolean")) snprintf(out, n, " %s%s=%d", name, tag, At<bool>(obj, off) ? 1 : 0);
    else if (!strcmp(tn, "UnityEngine.Vector3")) { const float *v = &At<float>(obj, off); snprintf(out, n, " %s=(%.2f,%.2f,%.2f)", name, v[0], v[1], v[2]); }
    else snprintf(out, n, " %s:%s=%s", name, tn[0] ? tn : "?", At<void *>(obj, off) ? "set" : "null");
}

static void LaneDebug(char *buf, size_t size) {
    static Il2CppClass *road = nullptr, *drag = nullptr, *dm = nullptr;
    static bool looked = false;
    if (!looked) { looked = true; road = FindClass("", "RoadChanger"); drag = FindClass("", "SwitchLaneDrag"); dm = FindClass("", "DragManager"); }
    size_t used = 0;
    auto add = [&](const char *t) { size_t l = strlen(t); if (used + l + 1 < size) { memcpy(buf + used, t, l); used += l; buf[used] = 0; } };
    buf[0] = 0;
    char tmp[160];
    Il2CppArray *roads = road ? FindAll(TypeOf(road)) : nullptr;
    void *r = FirstAlive(roads);
    snprintf(tmp, sizeof(tmp), "lanes: RoadChanger %s x%d", road ? "class ok" : "class missing", (int)Len(roads)); add(tmp);
    static const char *rf[] = { "currentLane", "laneNum", "oneLane", "shiftBy", "currentShift" };
    for (const char *f : rf) { FieldText(r, road, f, tmp, sizeof(tmp)); add(tmp); }
    // the function the shoe drag ends in: does it take an int, as expected?
    const MethodInfo *shift = road ? FindMethod(road, "shiftToLaneNumber", 1, "System.Int32") : nullptr;
    snprintf(tmp, sizeof(tmp), " shiftToLaneNumber(int)=%s", shift ? "yes" : (road && FindMethod(road, "shiftToLaneNumber", 1) ? "other-type" : "no")); add(tmp);
    Il2CppArray *drags = drag ? FindAll(TypeOf(drag)) : nullptr;
    snprintf(tmp, sizeof(tmp), " | SwitchLaneDrag %s x%d active", drag ? "class ok" : "class missing", (int)Len(drags)); add(tmp);
    void *d = dm ? FirstAlive(FindAll(TypeOf(dm))) : nullptr;
    add(" | DragManager");
    FieldText(d, dm, "firstLaneLeft", tmp, sizeof(tmp)); add(tmp);
    FieldText(d, dm, "secondLaneRight", tmp, sizeof(tmp)); add(tmp);
    snprintf(tmp, sizeof(tmp), " | other lane: on=%d home=%d now=%d tries=%d reverts=%d%s", gBF.laneOther ? 1 : 0, sLaneHome, LaneNow(), sLaneTries, sLaneReverts, sLaneGaveUp ? " GAVE UP" : ""); add(tmp);
    add(" | InventaryData");                        // which pin system the game uses
    FieldText(sInvData.get(), N.InvData, "UsePinHolder", tmp, sizeof(tmp)); add(tmp);
    snprintf(tmp, sizeof(tmp), " | oil far edge rows=%d scale=%.4f", sSizeYRows, sSizeYScale); add(tmp);
}

Str BFDebugInfo(void) {
    Str s;
    s += Fmt("BowlingPlus v%s (%s) | %s\n", BF_VERSION, BF_PLATFORM_VERSION, BFDeviceLine());
    s += Fmt("engine=%d settled=%d safe=%d mode=%d loc=%d offline=%d tutorial=%d frame=%d\n",
        gBFStatus.engineReady, sSettled, gBFSafeMode, sMode, sLoc, gBFStatus.offline, sInTutorial, sFrame);
    s += Fmt("cfg: skin=%d pinPhys=%d pinFric=%.2f speed=%.1f spare=%d auto=%d mask=0x%03x fps120=%d\n",
        gBF.textureFix, gBF.pinPhys, PinFriction(), gBF.speedMult, gBF.spareMode, gBF.spareAuto, gBF.lastPinMask, gBF.fps120);
    s += Fmt("%s | skinKind=%d\n", !sBallLine.empty() ? sBallLine : Str("no current ball"), (int)sCurSkin);
    bool live = N.ok && !gBFSafeMode && sSettled;
    ListView cat;
    void *vd = live ? VisualData() : nullptr;
    s += Fmt("catalog: %d entries, %lu app assets\n", (vd && ReadList(At<void *>(vd, N.ivd_items), cat)) ? cat.size : -1,
        (unsigned long)AppAssetNames().size());
    s += Fmt("pearl fix: fail=%d fixed=%d sheet=%d | game had: %s texc=%d | bp fixed=%d hasSkin=%d (game had %s)\n",
        sPearlFail, sPearlFixed, sPearlSheet, live ? PathInfo(sGamePearlDlc) : Str("-"), sGamePearlTexc, sBPFixed, sBPHasSkin,
        live && sBPLink.changed ? PathInfo(sBPLink.origDlc) : Str("-"));
    struct { const char *name; ObjRef *ref; } balls[] = { { "Pearl", &sPearlShop }, { "Hybrid", &sHybridShop }, { "BP", &sBPShop }, { "Solid", &sSolidShop } };
    for (auto &b : balls) {
        void *it = b.ref->get();
        if (!it) { s += Fmt("%s: not found\n", b.name); continue; }
        s += Fmt("%s: skin %s texc=%d\n", b.name, live ? PathInfo(SkinDlcOf(it)) : Str("-"), TexcOf(it));
    }
    s += Fmt("arsenal: query=%s shown=%d total=%d\n", sQuery.empty() ? Str("-") : sQuery, sShown, sTotal);
    int tgt = live && N.App_getFps ? InvokeInt(N.App_getFps, nullptr, nullptr, -1) : -1;
    s += Fmt("fps: on=%d applied=%d target=%d table(orig)=%d/%d screenMax=%d panelCap=%d | %s\n",
        gBF.fps120, sFpsApplied, tgt, sOrigMenuFps, sOrigGameFps, BFScreenMaxHz(), sOverlayRestoreFps, BFPrivacyDebug());
    {
        ListView ol;
        int nOils = OilList(ol) ? ol.size : -1;
        s += Fmt("oil textures ready=%d | thickness colors=%s | ", sPrewarmDone, sThickApplied ? "on" : "off");
        s += Fmt("oil: mirror=%d lanes=%d breakdown=%d redraws=%d invisible=%d picks=%d custom=%s applied=%s sel=%d patterns=%d %s\n",
            gBF.oilMirrorFix, sMirrorLanes, gBF.oilBreakdown, sBreakdownRedraws, gBF.oilInvisible, sInvisPicks,
            sCustomPattern.isNull() ? Str("-") : sCustomPattern["name"].str(), !sCustomAppliedId.empty() ? "yes" : "no",
            live ? OilSelected() : -1, nOils, sCustomNote);
        s += OilReport(live);
    }
    s += Fmt("pin image: on=%d materials=%d size=%d fails=%d\n", gBF.pinImage, sPinImgMats, sPinImgSize, sPinImgFails);
    s += Fmt("loading: unstick=%d rescued=%d spinner=%d | ipv4=%d dnsSlots=%d | ball cdm=%d\n",
        gBF.unstick, sUnstuckCount, sSpinnerCount, gBF.gameIPv4, BFDnsHookSlots(), BallCDM());
    {
        void *inv = SpinInventary();
        s += Fmt("pin tap: %s\n", sTapNote);
        s += Fmt("spin boost: x%.1f last %d -> %d rpm grip x%.2f | rpmFactor=%.3f maxOmega=%.1f\n", gBF.spinMult, sSpunFrom, sSpunTo, sGrip,
            (inv && N.inv_rpmFactor >= 0) ? At<float>(inv, N.inv_rpmFactor) : -1.f, (inv && N.inv_maxOmega >= 0) ? At<float>(inv, N.inv_maxOmega) : -1.f);
    }
    void *holder = sHolders[0].get();
    ListView lv;
    if (live && holder && N.ph_pins >= 0 && ReadList(At<void *>(holder, N.ph_pins), lv) && lv.size > 0 && lv.items[0]) {
        void *rb = GetComp(GameObjectOf(At<void *>(lv.items[0], N.pin_physic)), N.tRigidbody);
        if (rb) s += Fmt("pin1: cdm=%d kinematic=%d linDamp=%.3f angDamp=%.3f pins=%d\n",
                    InvokeInt(N.RB_getCDM, rb, nullptr, -1), InvokeBool(N.RB_isKinematic, rb, nullptr, false),
                    N.RB_getLinDamp ? InvokeFloat(N.RB_getLinDamp, rb, -1) : -1.f,
                    N.RB_getAngDamp ? InvokeFloat(N.RB_getAngDamp, rb, -1) : -1.f, lv.size);
    }
    { char lanes[1024]; LaneDebug(lanes, sizeof(lanes)); s += lanes; s += "\n"; TurnDebug(lanes, sizeof(lanes)); s += lanes; s += "\n"; BgDebug(lanes, sizeof(lanes)); s += lanes; s += "\n"; PinPhysDebug(lanes, sizeof(lanes)); s += lanes; s += "\n"; RateDebug(lanes, sizeof(lanes)); s += lanes; s += "\n"; NoTapDebug(lanes, sizeof(lanes)); s += lanes; s += "\n"; }
    return s;
}
