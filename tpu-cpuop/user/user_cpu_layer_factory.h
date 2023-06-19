#ifndef CPU_LAYER_FACTORY_H_
#define CPU_LAYER_FACTORY_H_

#include "user_cpu_layer.h"

namespace usercpu {

class user_cpu_layer;

class Registry {
public:
    typedef USER_STD::shared_ptr<user_cpu_layer> (*Creator)();
    typedef std::map<int, Creator> CreatorLayerRegistry;

    /* Create Layer Factor */
    static CreatorLayerRegistry& LayerRegistry() {
        static CreatorLayerRegistry g_registry_;
        return g_registry_;
    }

    /* Reigster Layer Creator to Factory */
    static void addCreator(const int layer_type, Creator Creator) {
        auto& layer_registry = LayerRegistry();

        if (layer_registry.count(layer_type)==0){
            //cout << "register layer_type: " << layer_type << endl;
            layer_registry[layer_type] = Creator;
        } else {
            cout << "[WARNING] layer_type:" << layer_type << " already registered on cpu" << endl;
        }
    }

    /* unified layer create interface : search layer factor and find corresponding layer creator */
    static USER_STD::shared_ptr<user_cpu_layer> createlayer(const int layer_type) {
        auto& layer_registry = LayerRegistry();
        if (layer_registry.count(layer_type) != 0) {
            return layer_registry[layer_type]();
        }
        printf("Cannot create user cpu layer for layer_type:%d\n", layer_type);
        exit(-1);
    }

private:
    Registry() {}

};


class Registerer {
public:
    Registerer(const int& layer_type,
                       USER_STD::shared_ptr<user_cpu_layer> (*Creator)()) {
        Registry::addCreator(layer_type, Creator);
    }
};


#define REGISTER_USER_CPULAYER_CLASS(layer_type, layer_name)           \
  USER_STD::shared_ptr<user_cpu_layer> Creator_##layer_name##Layer() {    \
    return USER_STD::shared_ptr<user_cpu_layer>(new layer_name##_layer()); \
  }                                                                      \
  static Registerer g_Creator_f_##type(layer_type, Creator_##layer_name##Layer);

} /* namespace usercpu */

#endif /* USER_CPU_LAYER_FACTORY_H_ */
