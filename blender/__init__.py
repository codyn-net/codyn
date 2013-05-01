bl_info = {
    "name": "Codyn Tools",
    "category": "Simulation"
}

import sys, math

if not "gi.repository.Cdn" in sys.modules:
    p = '/usr/lib/python3/dist-packages'
    sys.path.append(p)
    from gi.repository import Cdn
    sys.path = sys.path[:-1]

if "bpy" in locals():
    import imp
    imp.reload(importer)
    imp.reload(codyn)
    imp.reload(simulator)
else:
    import bpy
    from . import (importer, codyn, simulator)

def simulator_init():
    simulator.init()

def simulator_loop():
    simulator.loop()

def register():
    for i in range(1, 8):
        defval = [0 for x in range(0, i)]

        if i == 1:
            def make_prop(name, unit='NONE', update=None):
                return bpy.props.FloatProperty(name,
                                               default=0,
                                               unit=unit,
                                               step=0.1 * math.pi,
                                               update=update)

            nm = ''
        else:
            def make_prop(name, unit='NONE', update=None):
                return bpy.props.FloatVectorProperty(name,
                                                     default=defval,
                                                     size=i,
                                                     unit=unit,
                                                     step=0.1 * math.pi,
                                                     update=update)

            nm = str(i)

        qnm = "cdn_q{0}".format(nm)
        dqnm = "cdn_dq{0}".format(nm)
        ddqnm = "cdn_ddq{0}".format(nm)

        setattr(bpy.types.Object,
                qnm,
                make_prop("q{0}".format(nm),
                          unit='ROTATION',
                          update=codyn.variable_sync('q', i)))

        setattr(bpy.types.Object,
                dqnm,
                make_prop("dq{0}".format(nm),
                          unit='VELOCITY',
                          update=codyn.variable_sync('dq', i)))

        setattr(bpy.types.Object,
                ddqnm,
                make_prop("ddq{0}".format(nm),
                          unit='ACCELERATION',
                          update=codyn.variable_sync('ddq', i)))

    bpy.types.Object.cdn_mass = bpy.props.FloatProperty("mass")

    bpy.utils.register_class(importer.CodynImport)

def unregister():
    bpy.utils.unregister_class(importer.CodynImport)

if __name__ == "main":
    register()

# vi:ts=4:et