#include <Python.h>

#include <btBulletDynamicsCommon.h>

const char * letters = "ModernGL";
const int count = 8;
const int vertices = count * 36 + 6;
const btVector3 low = {-5.25, -1.5, 0.0};
const btVector3 high = {5.25, 2.5, 4.0};

static btDiscreteDynamicsWorld * world;
static btRigidBody * cubes[count];
static btRigidBody * picked;
static btPoint2PointConstraint * pick;
static double pick_distance;
static bool was_pressed;

static btRigidBody * body(btCollisionShape * shape, double mass, btVector3 position) {
    btVector3 inertia = {0.0, 0.0, 0.0};
    if (mass) {
        shape->calculateLocalInertia(mass, inertia);
    }
    btRigidBody * res = new btRigidBody(mass, NULL, shape, inertia);
    res->setWorldTransform(btTransform({0.0, 0.0, 0.0, 1.0}, position));
    res->setFriction(0.8);
    res->setRollingFriction(0.01);
    res->setSpinningFriction(0.01);
    world->addRigidBody(res);
    return res;
}

static PyObject * meth_init(PyObject * self, PyObject * args) {
    btDefaultCollisionConfiguration * configuration = new btDefaultCollisionConfiguration();
    btCollisionDispatcher * dispatcher = new btCollisionDispatcher(configuration);
    world = new btDiscreteDynamicsWorld(dispatcher, new btDbvtBroadphase(), new btSequentialImpulseConstraintSolver(), configuration);
    world->setGravity({0.0, 0.0, -20.0});
    body(new btStaticPlaneShape({0.0, 0.0, 1.0}, low.z()), 0.0, {0.0, 0.0, 0.0});
    body(new btStaticPlaneShape({0.0, 0.0, -1.0}, -high.z()), 0.0, {0.0, 0.0, 0.0});
    body(new btStaticPlaneShape({1.0, 0.0, 0.0}, low.x()), 0.0, {0.0, 0.0, 0.0});
    body(new btStaticPlaneShape({-1.0, 0.0, 0.0}, -high.x()), 0.0, {0.0, 0.0, 0.0});
    body(new btStaticPlaneShape({0.0, 1.0, 0.0}, low.y()), 0.0, {0.0, 0.0, 0.0});
    body(new btStaticPlaneShape({0.0, -1.0, 0.0}, -high.y()), 0.0, {0.0, 0.0, 0.0});
    btBoxShape * shape = new btBoxShape({0.5, 0.5, 0.5});
    for (int i = 0; i < count; ++i) {
        cubes[i] = body(shape, 1.0, {(i - 3.5) * 1.1, 0.0, 0.5});
    }
    Py_RETURN_NONE;
}

static void start_pick(btVector3 from, btVector3 dir) {
    btVector3 to = from + dir * 100.0;
    btCollisionWorld::ClosestRayResultCallback ray(from, to);
    ray.m_collisionFilterMask = btBroadphaseProxy::DefaultFilter;
    world->rayTest(from, to, ray);
    btRigidBody * hit = (btRigidBody *)btRigidBody::upcast(ray.m_collisionObject);
    if (!hit) {
        return;
    }
    picked = hit;
    picked->setActivationState(DISABLE_DEACTIVATION);
    picked->setDamping(0.0, 0.9);
    pick = new btPoint2PointConstraint(*picked, picked->getCenterOfMassTransform().inverse() * ray.m_hitPointWorld);
    pick->m_setting.m_impulseClamp = 30.0;
    pick->m_setting.m_tau = 0.001;
    world->addConstraint(pick, true);
    pick_distance = (ray.m_hitPointWorld - from).length();
}

static void stop_pick() {
    world->removeConstraint(pick);
    delete pick;
    picked->setDamping(0.0, 0.0);
    picked->forceActivationState(ACTIVE_TAG);
    picked->setDeactivationTime(0.0);
    picked = NULL;
    pick = NULL;
}

static PyObject * meth_update(PyObject * self, PyObject * args) {
    double dt;
    btVector3 from;
    btVector3 dir;
    int pressed;
    if (!PyArg_ParseTuple(args, "d(ddd)(ddd)p", &dt, &from[0], &from[1], &from[2], &dir[0], &dir[1], &dir[2], &pressed)) {
        return NULL;
    }
    if (pressed && !was_pressed) {
        start_pick(from, dir);
    }
    if (!pressed && picked) {
        stop_pick();
    }
    if (picked) {
        btVector3 target = from + dir * pick_distance;
        target.setMax(low + btVector3(0.5, 0.5, 0.5));
        target.setMin(high - btVector3(0.5, 0.5, 0.5));
        pick->setPivotB(target);
    }
    was_pressed = pressed;
    world->stepSimulation(dt, 8, 1.0 / 120.0);
    Py_RETURN_NONE;
}

static float * write(float * ptr, btVector3 position, btVector3 normal, double u, double v, int layer) {
    *ptr++ = (float)position.x();
    *ptr++ = (float)position.y();
    *ptr++ = (float)position.z();
    *ptr++ = (float)normal.x();
    *ptr++ = (float)normal.y();
    *ptr++ = (float)normal.z();
    *ptr++ = (float)u;
    *ptr++ = (float)v;
    *ptr++ = (float)layer;
    return ptr;
}

static PyObject * meth_mesh(PyObject * self, PyObject * args) {
    const btVector3 faces[6][2] = {
        {{1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}},
        {{-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}},
        {{0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}},
        {{0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}},
        {{0.0, 0.0, 1.0}, {0.0, 1.0, 0.0}},
        {{0.0, 0.0, -1.0}, {0.0, -1.0, 0.0}},
    };
    const double corners[6][2] = {{-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0}};

    PyObject * res = PyBytes_FromStringAndSize(NULL, vertices * 9 * 4);
    float * ptr = (float *)PyBytes_AsString(res);

    for (int i = 0; i < count; ++i) {
        btTransform t = cubes[i]->getWorldTransform();
        for (int f = 0; f < 6; ++f) {
            btVector3 n = faces[f][0];
            btVector3 up = faces[f][1];
            btVector3 right = up.cross(n);
            for (int k = 0; k < 6; ++k) {
                double a = corners[k][0];
                double b = corners[k][1];
                btVector3 position = t * ((n + right * a + up * b) * 0.5);
                ptr = write(ptr, position, t.getBasis() * n, (a + 1.0) * 0.5, (1.0 - b) * 0.5, i);
            }
        }
    }

    for (int k = 0; k < 6; ++k) {
        double a = corners[k][0] * 40.0;
        double b = corners[k][1] * 40.0;
        ptr = write(ptr, {a, b, 0.0}, {0.0, 0.0, 1.0}, a * 0.5, b * 0.5, count);
    }

    return res;
}

static PyMethodDef module_methods[] = {
    {"init", (PyCFunction)meth_init, METH_NOARGS},
    {"update", (PyCFunction)meth_update, METH_VARARGS},
    {"mesh", (PyCFunction)meth_mesh, METH_NOARGS},
    {},
};

static PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "toys", NULL, -1, module_methods};

extern "C" PyObject * PyInit_toys() {
    PyObject * module = PyModule_Create(&module_def);
    PyModule_AddIntConstant(module, "vertices", vertices);
    PyModule_AddStringConstant(module, "letters", letters);
    return module;
}
