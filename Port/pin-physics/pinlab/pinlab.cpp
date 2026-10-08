// pinlab: the game's pin physics rebuilt on PhysX 4.1 (the engine Unity 6 embeds), with the game's own data:
// pin colliders (5 convex hulls + 2 capsules, from level1/sharedassets1), masses, centre of mass and inertia
// (RunPsycsTest), materials and combine modes (Minimum), the deck/lane/kickback/gutter/pit boxes, the rack
// (Constants: 12-inch triangle, head pin at y 18.263), and the scene settings from PhysicsManager/TimeManager.
// Experiments (argv[1]): ballpin, pinpin, rack, rest. Output: CSV on stdout.
#include "PxPhysicsAPI.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <fstream>
#include <sstream>
using namespace physx;

static PxDefaultAllocator gAlloc;
static PxDefaultErrorCallback gErr;
static PxFoundation *gFound;
static PxPhysics *gPhys;
static PxCooking *gCook;
static PxDefaultCpuDispatcher *gDisp;

// ---------------- scene data ----------------
struct Hull { std::string mat; std::vector<PxVec3> v; PxConvexMesh *mesh = nullptr; };
struct Cap { std::string mat; float r, h; PxVec3 c; int dir; };
struct Box { std::string name, mat; PxVec3 c, half; PxQuat q; };
struct Mat { float sf, df, e; };
static std::vector<Hull> gHulls;
static std::vector<Cap> gCaps;
static std::vector<Box> gBoxes;
static std::map<std::string, Mat> gMatData;

static void LoadScene(const char *path) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream s(line);
        std::string kind;
        s >> kind;
        if (kind == "hull") {
            Hull h; std::string go; int n;
            s >> go >> h.mat >> n;
            for (int i = 0; i < n; i++) { std::getline(f, line); std::istringstream p(line); PxVec3 v; p >> v.x >> v.y >> v.z; h.v.push_back(v); }
            gHulls.push_back(h);
        } else if (kind == "capsule") {
            Cap c; std::string go;
            s >> go >> c.mat >> c.r >> c.h >> c.c.x >> c.c.y >> c.c.z >> c.dir;
            gCaps.push_back(c);
        } else if (kind == "box") {
            Box b;
            s >> b.name >> b.mat >> b.c.x >> b.c.y >> b.c.z >> b.half.x >> b.half.y >> b.half.z >> b.q.x >> b.q.y >> b.q.z >> b.q.w;
            gBoxes.push_back(b);
        } else if (kind == "material") {
            std::string n; Mat m; int fc, bc;
            s >> n >> m.sf >> m.df >> m.e >> fc >> bc;
            gMatData[n] = m;
        }
    }
}

// ---------------- configuration ----------------
struct Cfg {
    const char *name = "stock";
    float dt = 0.0075f;               // TimeManager fixed timestep (1058399 / 141120000)
    int posIters = 7, velIters = 7;    // PhysicsManager defaults
    float contactOffset = 0.0005f;     // PhysicsManager default contact offset
    float bounceThreshold = 0.03f;
    float sleepThreshold = 0.01f;
    int pinCdm = 0, ballCdm = 0;       // Unity CollisionDetectionMode: 0 discrete, 1 continuous, 2 continuous dynamic, 3 speculative
    float pinDepen = 1e5f, ballDepen = 1e5f;   // InventaryData.ResetRigidBody sets 100000 for pins and the ball
    float pinMaxAng = 1e5f;            // ...and maxAngularVelocity 100000
    float pinDamp = 0, pinAngDamp = 0;
    float pinRestitution = -1;         // -1: the game's (not settable at runtime: PhysicsMaterial.set_bounciness is stripped)
    float pinPinFriction = -1;         // -1: the game's
    float deckFriction = -1;           // -1: the game's (Pindeck 0.9 static / 0.6 dynamic; settable at runtime)
};

static std::map<std::string, PxMaterial *> gMats;
static PxMaterial *GetMat(const std::string &n, const Cfg &c) {
    std::string key = n + "|" + std::to_string(c.pinRestitution) + "|" + std::to_string(c.pinPinFriction) + "|" + std::to_string(c.deckFriction);
    auto it = gMats.find(key);
    if (it != gMats.end()) return it->second;
    Mat m = gMatData.count(n) ? gMatData[n] : Mat{ 0.3f, 0.3f, 0.65f };
    bool pin = n == "Pin" || n == "PinButtom";
    if (pin && c.pinRestitution >= 0) m.e = c.pinRestitution * (n == "PinButtom" ? 0.4f / 0.65f : 1.0f);
    if (pin && c.pinPinFriction >= 0) { m.df = c.pinPinFriction; m.sf = c.pinPinFriction; }
    if (n == "Pindeck" && c.deckFriction >= 0) { m.df = c.deckFriction; m.sf = c.deckFriction; }
    PxMaterial *mat = gPhys->createMaterial(m.sf, m.df, m.e);
    mat->setFrictionCombineMode(PxCombineMode::eMIN);       // Unity "Minimum" (all the game's materials)
    mat->setRestitutionCombineMode(PxCombineMode::eMIN);
    gMats[key] = mat;
    return mat;
}

static PxFilterFlags Shader(PxFilterObjectAttributes a0, PxFilterData, PxFilterObjectAttributes a1, PxFilterData, PxPairFlags &pf, const void *, PxU32) {
    pf = PxPairFlag::eCONTACT_DEFAULT | PxPairFlag::eDETECT_CCD_CONTACT;
    return PxFilterFlag::eDEFAULT;
}

struct World {
    Cfg cfg;
    PxScene *scene = nullptr;
    std::vector<PxRigidDynamic *> pins;
    PxRigidDynamic *ball = nullptr;
    ~World() { if (scene) scene->release(); }
};

static void ApplyCdm(PxRigidDynamic *b, int cdm) {
    b->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, cdm == 1 || cdm == 2);
    b->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD, cdm == 3);
}

static void MakeScene(World &w) {
    PxSceneDesc sd(gPhys->getTolerancesScale());
    sd.gravity = PxVec3(0, 0, -9.81f);                     // Z is up in this game
    sd.cpuDispatcher = gDisp;
    sd.filterShader = Shader;
    sd.bounceThresholdVelocity = w.cfg.bounceThreshold;
    sd.solverType = PxSolverType::ePGS;                     // PhysicsManager solver type 0
    sd.frictionType = PxFrictionType::ePATCH;               // friction type 0
    sd.flags |= PxSceneFlag::eENABLE_PCM | PxSceneFlag::eENABLE_CCD;
    w.scene = gPhys->createScene(sd);
    for (auto &b : gBoxes) {
        PxRigidStatic *s = gPhys->createRigidStatic(PxTransform(b.c, b.q));
        PxShape *sh = PxRigidActorExt::createExclusiveShape(*s, PxBoxGeometry(b.half), *GetMat(b.mat, w.cfg));
        sh->setContactOffset(w.cfg.contactOffset);
        sh->setRestOffset(0);
        w.scene->addActor(*s);
    }
}

static PxRigidDynamic *AddPin(World &w, const PxTransform &pose) {
    PxRigidDynamic *b = gPhys->createRigidDynamic(pose);
    for (auto &h : gHulls) {
        if (!h.mesh) {
            PxConvexMeshDesc d;
            d.points.count = (PxU32)h.v.size(); d.points.stride = sizeof(PxVec3); d.points.data = h.v.data();
            d.flags = PxConvexFlag::eCOMPUTE_CONVEX;
            h.mesh = gCook->createConvexMesh(d, gPhys->getPhysicsInsertionCallback());
        }
        PxShape *sh = PxRigidActorExt::createExclusiveShape(*b, PxConvexMeshGeometry(h.mesh), *GetMat(h.mat, w.cfg));
        sh->setContactOffset(w.cfg.contactOffset); sh->setRestOffset(0);
    }
    for (auto &c : gCaps) {                                  // Unity capsule height includes the caps; PhysX's axis is X
        float hh = std::max(0.0f, c.h * 0.5f - c.r);
        PxShape *sh = PxRigidActorExt::createExclusiveShape(*b, PxCapsuleGeometry(c.r, hh), *GetMat(c.mat, w.cfg));
        sh->setLocalPose(PxTransform(c.c, PxQuat(-PxHalfPi, PxVec3(0, 1, 0))));   // X -> Z (dir 2)
        sh->setContactOffset(w.cfg.contactOffset); sh->setRestOffset(0);
    }
    b->setMass(1.644f);
    b->setCMassLocalPose(PxTransform(PxVec3(0, 0, 0.1498f)));                   // RunPsycsTest.pinCenterMass
    b->setMassSpaceInertiaTensor(PxVec3(0.013934150f, 0.013934150f, 0.0019148530f));   // pinInertiaTensor
    b->setLinearDamping(w.cfg.pinDamp); b->setAngularDamping(w.cfg.pinAngDamp);
    b->setMaxDepenetrationVelocity(w.cfg.pinDepen);
    b->setMaxAngularVelocity(w.cfg.pinMaxAng);
    b->setSleepThreshold(w.cfg.sleepThreshold);
    b->setSolverIterationCounts(w.cfg.posIters, w.cfg.velIters);
    ApplyCdm(b, w.cfg.pinCdm);
    w.scene->addActor(*b);
    w.pins.push_back(b);
    return b;
}

static PxRigidDynamic *AddBall(World &w, const PxVec3 &p, float mass) {
    PxRigidDynamic *b = gPhys->createRigidDynamic(PxTransform(p));
    PxShape *sh = PxRigidActorExt::createExclusiveShape(*b, PxSphereGeometry(0.108f), *GetMat("Ball", w.cfg));
    sh->setContactOffset(w.cfg.contactOffset); sh->setRestOffset(0);
    PxRigidBodyExt::setMassAndUpdateInertia(*b, mass);
    b->setMaxDepenetrationVelocity(w.cfg.ballDepen);
    b->setMaxAngularVelocity(1e5f);
    b->setSleepThreshold(w.cfg.sleepThreshold);
    b->setSolverIterationCounts(w.cfg.posIters, w.cfg.velIters);
    ApplyCdm(b, w.cfg.ballCdm);
    w.scene->addActor(*b);
    w.ball = b;
    return b;
}

// the rack (Constants: step (0.1524, 0.26396), head pin y 18.263, standing z -0.0005)
static PxVec3 PinSpot(int i) {
    static const float m[10][2] = { { 0, 0 }, { 1, 1 }, { -1, 1 }, { 2, 2 }, { 0, 2 }, { -2, 2 }, { 3, 3 }, { 1, 3 }, { -1, 3 }, { -3, 3 } };
    return PxVec3(0.15239999f * m[i][0], 18.263f + 0.26396456f * m[i][1], -0.0005f);
}

static float Tilt(PxRigidDynamic *b) {                   // degrees between the pin's axis and world up
    PxVec3 up = b->getGlobalPose().q.rotate(PxVec3(0, 0, 1));
    return acosf(std::max(-1.0f, std::min(1.0f, up.z))) * 57.29578f;
}

// deepest overlap between two pins (all shape pairs), metres
static float PinOverlap(PxRigidDynamic *a, PxRigidDynamic *b) {
    PxShape *sa[8], *sb[8];
    int na = a->getShapes(sa, 8), nb = b->getShapes(sb, 8);
    float worst = 0;
    PxTransform pa = a->getGlobalPose(), pb = b->getGlobalPose();
    for (int i = 0; i < na; i++)
        for (int j = 0; j < nb; j++) {
            PxVec3 dir; PxF32 depth;
            PxGeometryHolder ga = sa[i]->getGeometry(), gb = sb[j]->getGeometry();
            if (PxGeometryQuery::computePenetration(dir, depth, ga.any(), pa * sa[i]->getLocalPose(), gb.any(), pb * sb[j]->getLocalPose()))
                worst = std::max(worst, (float)depth);
        }
    return worst;
}

static void Step(World &w) { w.scene->simulate(w.cfg.dt); w.scene->fetchResults(true); }

// ---------------- configurations ----------------
static Cfg Named(const std::string &n) {
    Cfg c;
    if (n == "stock") { c.name = "stock"; }                                               // the game as shipped
    else if (n == "bp") { c.name = "bp"; c.ballCdm = 2; }                                  // BowlingPlus today: ball CCD (pin fix misses the lane pins)
    else if (n == "ccd") { c.name = "ccd"; c.ballCdm = 2; c.pinCdm = 2; }                  // + lane pins ContinuousDynamic
    else if (n == "spec") { c.name = "spec"; c.ballCdm = 2; c.pinCdm = 3; }                // + lane pins speculative
    else if (n == "ccd_dp") { c.name = "ccd_dp"; c.ballCdm = 2; c.pinCdm = 2; c.pinDepen = 3.0f; }   // + pins' max depenetration 3 m/s
    else if (n == "spec_dp") { c.name = "spec_dp"; c.ballCdm = 2; c.pinCdm = 3; c.pinDepen = 3.0f; }
    else if (n == "dt2") { c.name = "dt2"; c.ballCdm = 2; c.pinCdm = 2; c.dt = 0.0025f; }  // not reachable at runtime (no fixedDeltaTime setter)
    else if (n == "it20") { c.name = "it20"; c.ballCdm = 2; c.pinCdm = 2; c.posIters = 20; c.velIters = 10; }   // not reachable (setters stripped)
    else if (n == "co5") { c.name = "co5"; c.ballCdm = 2; c.contactOffset = 0.005f; }       // contact offset 5 mm (not settable at runtime)
    else if (n == "co10") { c.name = "co10"; c.ballCdm = 2; c.contactOffset = 0.01f; }     // Unity's default contact offset
    else if (n == "dt375") { c.name = "dt375"; c.ballCdm = 2; c.dt = 0.00375f; }            // 2x physics rate (timestep, data write only)
    else if (n == "dt25") { c.name = "dt25"; c.ballCdm = 2; c.dt = 0.0025f; }               // 3x physics rate
    else if (n == "dt375co5") { c.name = "dt375co5"; c.ballCdm = 2; c.dt = 0.00375f; c.contactOffset = 0.005f; }
    else if (n == "dt375_pf02") { c = Named("dt375"); c.name = "dt375_pf02"; c.pinPinFriction = 0.2f; }
    else if (n == "dt375_pf035") { c = Named("dt375"); c.name = "dt375_pf035"; c.pinPinFriction = 0.35f; }
    else if (n == "dt375_deck03") { c = Named("dt375"); c.name = "dt375_deck03"; c.deckFriction = 0.3f; }
    else if (n == "dt375_e75") { c = Named("dt375"); c.name = "dt375_e75"; c.pinRestitution = 0.75f; }
    else if (n == "stock_pf02") { c.name = "stock_pf02"; c.pinPinFriction = 0.2f; }
    else if (n == "dt375_pf025") { c = Named("dt375"); c.name = "dt375_pf025"; c.pinPinFriction = 0.25f; }
    else if (n == "dt375_pf03") { c = Named("dt375"); c.name = "dt375_pf03"; c.pinPinFriction = 0.30f; }
    else if (n == "dt375_pf04") { c = Named("dt375"); c.name = "dt375_pf04"; c.pinPinFriction = 0.40f; }
    else if (n == "stock_pf025") { c.name = "stock_pf025"; c.ballCdm = 2; c.pinPinFriction = 0.25f; }
    else if (n == "stock_pf03") { c.name = "stock_pf03"; c.ballCdm = 2; c.pinPinFriction = 0.3f; }
    else if (n == "ref") { c.name = "ref"; c.dt = 0.0005f; c.posIters = 20; c.velIters = 10; c.ballCdm = 2; c.pinCdm = 2; }   // converged reference
    else { fprintf(stderr, "unknown cfg %s\n", n.c_str()); exit(1); }
    return c;
}

// ---------------- experiment 1: ball onto one standing pin ----------------
// Ball rolling (pure roll) at v toward the head pin, centre offset `off` sideways. Records the pin's peak speed,
// its direction, the ball's deflection, and the deepest ball-pin overlap.
static void ExpBallPin(const Cfg &cfg, float v, float off, float mass, float &pinSpeed, float &pinAngle, float &ballDefl, float &overlap, float &tiltAt1s) {
    World w; w.cfg = cfg; MakeScene(w);
    PxRigidDynamic *pin = AddPin(w, PxTransform(PinSpot(0)));
    PxVec3 start(off, PinSpot(0).y - 1.2f, 0.108f);
    PxRigidDynamic *ball = AddBall(w, start, mass);
    ball->setLinearVelocity(PxVec3(0, v, 0));
    ball->setAngularVelocity(PxVec3(-v / 0.108f, 0, 0));   // rolling down +Y: contact point at rest needs wx = -v/r
    pinSpeed = 0; PxVec3 pv(0); overlap = 0; PxVec3 bv0 = ball->getLinearVelocity();
    int steps = (int)(1.0f / cfg.dt + 0.5f);
    for (int s = 0; s < steps; s++) {
        Step(w);
        PxVec3 lv = pin->getLinearVelocity();
        if (lv.magnitude() > pinSpeed) { pinSpeed = lv.magnitude(); pv = lv; }
        PxShape *ps[8], *bs; int n = pin->getShapes(ps, 8); ball->getShapes(&bs, 1);
        for (int i = 0; i < n; i++) {
            PxVec3 dir; PxF32 depth;
            PxGeometryHolder g1 = ps[i]->getGeometry(), g2 = bs->getGeometry();
            if (PxGeometryQuery::computePenetration(dir, depth, g1.any(), pin->getGlobalPose() * ps[i]->getLocalPose(), g2.any(), ball->getGlobalPose() * bs->getLocalPose()))
                overlap = std::max(overlap, (float)depth);
        }
    }
    pinAngle = atan2f(pv.x, pv.y) * 57.29578f;
    PxVec3 bv = ball->getLinearVelocity();
    ballDefl = atan2f(bv.x, bv.y) * 57.29578f;
    tiltAt1s = Tilt(pin);
    (void)bv0;
}

// ---------------- experiment 2: a moving pin onto a standing pin ----------------
// kinds: 0 upright sliding, 1 lying broadside (axis along x) flying, 2 lying head-first (axis along y), 3 tumbling end over end
static void ExpPinPin(const Cfg &cfg, int kind, float v, float off, float h, float spin,
                      float &tSpeed, float &transfer, float &overlap, float &energyGain, int &knocked, int &missed) {
    World w; w.cfg = cfg; MakeScene(w);
    PxVec3 target = PinSpot(0);
    PxRigidDynamic *tp = AddPin(w, PxTransform(target));
    PxVec3 vel(0, v, 0), ang(0);
    float back = 0.45f;
    PxTransform pose;
    if (kind == 0) pose = PxTransform(PxVec3(off, target.y - back, -0.0005f));
    else {
        PxVec3 axis = kind == 1 ? PxVec3(1, 0, 0) : kind == 2 ? PxVec3(0, 1, 0) : PxVec3(0, 1, 1).getNormalized();
        PxQuat q = PxShortestRotation(PxVec3(0, 0, 1), axis);          // the pin's long axis (local +Z) -> axis
        PxVec3 com(off, target.y - back - (kind == 2 ? 0.2f : 0.0f), h);
        pose = PxTransform(com - q.rotate(PxVec3(0, 0, 0.1498f)), q);  // place the centre of mass there
        if (kind == 1) ang = PxVec3(spin, 0, 0);                        // broadside, spinning about its own axis
        if (kind == 2) ang = PxVec3(0, spin, 0);                        // head first, spinning about its own axis
        if (kind == 3) ang = PxVec3(spin, 0, 0);                        // tilted 45 degrees, tumbling end over end
    }
    PxRigidDynamic *pp = AddPin(w, pose);
    pp->setLinearVelocity(vel);
    pp->setAngularVelocity(ang);
    if (kind != 0) { pp->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, false); }
    float m = 1.644f;
    auto KE = [&](PxRigidDynamic *b) {
        PxVec3 lv = b->getLinearVelocity(), av = b->getAngularVelocity();
        PxVec3 I = b->getMassSpaceInertiaTensor();
        PxQuat q = b->getGlobalPose().q * b->getCMassLocalPose().q;
        PxVec3 al = q.rotateInv(av);
        return 0.5f * m * lv.magnitudeSquared() + 0.5f * (I.x * al.x * al.x + I.y * al.y * al.y + I.z * al.z * al.z);
    };
    auto PE = [&](PxRigidDynamic *b) { return m * 9.81f * (b->getGlobalPose() * b->getCMassLocalPose()).p.z; };
    float ke0 = KE(pp) + KE(tp);
    float e0 = ke0 + PE(pp) + PE(tp);
    PxVec3 p0 = pp->getLinearVelocity() * m;
    tSpeed = 0; overlap = 0; energyGain = 0;
    float eMax = 0;
    int steps = (int)(0.5f / cfg.dt + 0.5f);
    for (int s = 0; s < steps; s++) {
        Step(w);
        tSpeed = std::max(tSpeed, tp->getLinearVelocity().magnitude());
        overlap = std::max(overlap, PinOverlap(pp, tp));
        // energy right after contact (gravity adds energy over time, so compare within the first 0.12 s)
        if (s * cfg.dt < 0.12f) eMax = std::max(eMax, KE(pp) + KE(tp) + PE(pp) + PE(tp));
    }
    transfer = (tSpeed * m) / std::max(1e-6f, p0.magnitude());
    energyGain = (eMax - e0) / std::max(1e-6f, ke0);   // energy created by the contact, as a share of the starting kinetic energy
    for (int s = 0; s < (int)(1.0f / cfg.dt); s++) Step(w);
    knocked = Tilt(tp) > 45.0f;
    missed = tSpeed < 0.05f;
}

// ---------------- experiment 3: full rack ----------------
static void ExpRack(const Cfg &cfg, float v, float boardOff, float entryDeg, float rpm, float mass, unsigned seed, int &down, unsigned &leaveMask) {
    World w; w.cfg = cfg; MakeScene(w);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> turn(0, 6.2831853f);
    for (int i = 0; i < 10; i++) AddPin(w, PxTransform(PinSpot(i), PxQuat(turn(rng), PxVec3(0, 0, 1))));
    // ball heading into the pocket from the right (1-3 side is -x): entry angle toward +x (left)
    float a = entryDeg * 0.0174533f;
    PxVec3 dir(sinf(a), cosf(a), 0);
    PxVec3 hit(boardOff, PinSpot(0).y, 0.108f);
    PxVec3 start = hit - dir * 1.0f;
    PxRigidDynamic *ball = AddBall(w, start, mass);
    ball->setLinearVelocity(dir * v);
    // rolling about the axis perpendicular to travel, plus some axis tilt (side rotation) from rpm
    float wroll = v / 0.108f, wtot = rpm * 0.10472f;
    PxVec3 rollAxis = PxVec3(0, 0, 1).cross(dir).getNormalized() * -1.0f;   // rolling forward
    PxVec3 side = PxVec3(0, 0, 1) * 0.0f + dir;                              // spin component about the travel direction (hook)
    float wside = std::max(0.0f, std::sqrt(std::max(0.0f, wtot * wtot - wroll * wroll)));
    ball->setAngularVelocity(rollAxis * (-wroll) + side * wside);
    for (int s = 0; s < (int)(4.0f / cfg.dt); s++) Step(w);
    down = 0; leaveMask = 0;
    for (int i = 0; i < 10; i++) {
        PxVec3 p = w.pins[i]->getGlobalPose().p;
        bool standing = Tilt(w.pins[i]) < 15.0f && fabsf(p.z) < 0.03f && fabsf(p.x - PinSpot(i).x) < 0.5f && p.y < 19.2f;
        if (standing) leaveMask |= 1u << i; else down++;
    }
}

// ---------------- experiment 5: USBC Bowlscore, rebuilt ----------------
// A rolling ball (pure roll, like a ramp) into a full rack at an entry angle (toward the 1-3 pocket) and an offset:
// the sideways distance between the ball's centre and the head pin's centre when they touch (0 to 5.5 inches).
static void ExpBowlscore(const Cfg &cfg, float v, float offIn, float entryDeg, float mass, unsigned seed, int &down, unsigned &leave) {
    World w; w.cfg = cfg; MakeScene(w);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> turn(0, 6.2831853f);
    for (int i = 0; i < 10; i++) AddPin(w, PxTransform(PinSpot(i), PxQuat(turn(rng), PxVec3(0, 0, 1))));
    float a = entryDeg * 0.0174533f;
    PxVec3 dir(sinf(a), cosf(a), 0);                       // heading up-lane and toward +x (the 1-3 pocket is on -x)
    float off = offIn * 0.0254f, reach = 0.108f + 0.0599f;
    PxVec3 head = PinSpot(0);
    PxVec3 touch(head.x - off, head.y - sqrtf(std::max(0.0f, reach * reach - off * off)), 0.108f);   // ball centre at contact
    // the straight path through that point; start 1 m back along it
    PxRigidDynamic *ball = AddBall(w, touch - dir * 1.0f, mass);
    ball->setLinearVelocity(dir * v);
    ball->setAngularVelocity(PxVec3(0, 0, 1).cross(dir) * (v / 0.108f));   // pure roll
    for (int s = 0; s < (int)(4.0f / cfg.dt); s++) Step(w);
    down = 0; leave = 0;
    for (int i = 0; i < 10; i++) {
        PxVec3 p = w.pins[i]->getGlobalPose().p;
        bool standing = Tilt(w.pins[i]) < 15.0f && fabsf(p.z) < 0.03f && fabsf(p.x - PinSpot(i).x) < 0.5f && p.y < 19.2f;
        if (standing) leave |= 1u << i; else down++;
    }
}

// ---------------- experiment 4: a standing rack left alone (stability) ----------------
static void ExpRest(const Cfg &cfg, float &maxDrift, float &maxTilt) {
    World w; w.cfg = cfg; MakeScene(w);
    for (int i = 0; i < 10; i++) AddPin(w, PxTransform(PinSpot(i)));
    for (int s = 0; s < (int)(5.0f / cfg.dt); s++) Step(w);
    maxDrift = 0; maxTilt = 0;
    for (int i = 0; i < 10; i++) {
        PxVec3 p = w.pins[i]->getGlobalPose().p;
        maxDrift = std::max(maxDrift, (PxVec3(p.x, p.y, 0) - PxVec3(PinSpot(i).x, PinSpot(i).y, 0)).magnitude());
        maxTilt = std::max(maxTilt, Tilt(w.pins[i]));
    }
}

int main(int argc, char **argv) {
    gFound = PxCreateFoundation(PX_PHYSICS_VERSION, gAlloc, gErr);
    PxTolerancesScale sc;
    gPhys = PxCreatePhysics(PX_PHYSICS_VERSION, *gFound, sc, false, nullptr);
    gCook = PxCreateCooking(PX_PHYSICS_VERSION, *gFound, PxCookingParams(sc));
    gDisp = PxDefaultCpuDispatcherCreate(0);
    LoadScene(argc > 2 ? argv[2] : "scene.txt");
    std::string exp = argc > 1 ? argv[1] : "ballpin";
    std::vector<std::string> cfgs;
    for (int i = 3; i < argc; i++) cfgs.push_back(argv[i]);
    if (cfgs.empty()) cfgs = { "stock", "bp", "ccd", "spec", "ccd_dp", "ref" };
    if (exp == "ballpin") {
        printf("cfg,v,mass,offset,pinSpeed,pinAngle,ballDefl,overlap_mm,tilt1s\n");
        for (auto &cn : cfgs) {
            Cfg c = Named(cn);
            for (float v : { 7.5f, 9.0f, 10.3f })
                for (float mass : { 5.437f, 6.80f })
                    for (float off = -0.16f; off <= 0.1601f; off += 0.02f) {
                        float ps, pa, bd, ov, tl;
                        ExpBallPin(c, v, off, mass, ps, pa, bd, ov, tl);
                        printf("%s,%.1f,%.3f,%.3f,%.4f,%.2f,%.2f,%.2f,%.1f\n", c.name, v, mass, off, ps, pa, bd, ov * 1000, tl);
                    }
        }
    } else if (exp == "pinpin") {
        printf("cfg,kind,v,offset,height,spin,tSpeed,transfer,overlap_mm,energyGain,knocked,missed\n");
        for (auto &cn : cfgs) {
            Cfg c = Named(cn);
            for (int kind = 0; kind < 4; kind++)
                for (float v : { 1.5f, 3.0f, 5.0f, 7.0f })
                    for (float off = -0.12f; off <= 0.1201f; off += 0.03f)
                        for (float spin : { 0.0f, 20.0f, 45.0f }) {
                            if (kind == 0 && spin > 0) continue;
                            float h = kind == 0 ? 0 : 0.12f;
                            float ts, tr, ov, eg; int kn, mi;
                            ExpPinPin(c, kind, v, off, h, spin, ts, tr, ov, eg, kn, mi);
                            printf("%s,%d,%.1f,%.3f,%.3f,%.0f,%.4f,%.4f,%.2f,%.3f,%d,%d\n", c.name, kind, v, off, h, spin, ts, tr, ov * 1000, eg, kn, mi);
                        }
        }
    } else if (exp == "rack") {
        int n = getenv("PINLAB_N") ? atoi(getenv("PINLAB_N")) : 60;
        printf("cfg,seed,v,board,entry,rpm,down,leave\n");
        for (auto &cn : cfgs) {
            Cfg c = Named(cn);
            for (int s = 0; s < n; s++) {
                std::mt19937 r(1000 + s);
                std::uniform_real_distribution<float> u(0, 1);
                float v = 7.6f + 1.8f * u(r);                      // 17..21 mph
                float board = -0.075f - 0.06f * u(r);               // around the 1-3 pocket (ball centre left of the 3 pin... -x side)
                float entry = 2.0f + 5.0f * u(r);                   // 2..7 degrees
                float rpm = 250 + 250 * u(r);
                int down; unsigned leave;
                ExpRack(c, v, board, entry, rpm, 6.80f, 5000 + s, down, leave);
                printf("%s,%d,%.2f,%.3f,%.2f,%.0f,%d,%u\n", c.name, s, v, board, entry, rpm, down, leave);
                fflush(stdout);
            }
        }
    } else if (exp == "bowlscore") {
        printf("cfg,offset_in,entry,shot,down,leave\n");
        int shots = getenv("PINLAB_SHOTS") ? atoi(getenv("PINLAB_SHOTS")) : 2;
        float v = getenv("PINLAB_V") ? atof(getenv("PINLAB_V")) : 8.0f;
        for (auto &cn : cfgs) {
            Cfg c = Named(cn);
            for (int oi = 0; oi <= 22; oi++)
                for (int ang = 0; ang <= 10; ang++)
                    for (int sh = 0; sh < shots; sh++) {
                        int down; unsigned leave;
                        ExpBowlscore(c, v, oi * 0.25f, (float)ang, 6.80f, (getenv("PINLAB_SEED") ? atoi(getenv("PINLAB_SEED")) : 77) + oi * 1000 + ang * 10 + sh, down, leave);
                        printf("%s,%.2f,%d,%d,%d,%u\n", c.name, oi * 0.25f, ang, sh, down, leave);
                    }
            fflush(stdout);
        }
    } else if (exp == "rest") {
        printf("cfg,maxDrift_mm,maxTilt_deg\n");
        for (auto &cn : cfgs) { Cfg c = Named(cn); float d, t; ExpRest(c, d, t); printf("%s,%.3f,%.3f\n", c.name, d * 1000, t); }
    }
    return 0;
}
