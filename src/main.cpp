#include <fstream>
#include <vector>
#include <optional>
#include <tuple>
#include <utility>
#include "cereal/cereal.hpp"
#include "cereal/archives/xml.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/tuple.hpp"
#include "cereal/types/utility.hpp"
#include "cereal/archives/portable_binary.hpp"
#include "cereal/archives/binary.hpp"
#include "entt.hpp"

namespace cereal {
    template <class Archive, typename T>
    inline void save(Archive& archive, const std::optional<T>& optional) {
        bool has_value = optional.has_value();
        archive(has_value);
        if (has_value) {
            archive(*optional);
        }
    }

    template <class Archive, typename T>
    inline void load(Archive& archive, std::optional<T>& optional) {
        bool has_value;
        archive(has_value);
        if (has_value) {
            T value;
            archive(value);
            optional.emplace(std::move(value));
        } else {
            optional.reset();
        }
    }
}  // namespace cereal

template <typename...>
struct type_list {};

template <typename Components, typename ContextVars>
struct RegistryArchive;

template <typename... Components, typename... ContextVars>
struct RegistryArchive<type_list<Components...>, type_list<ContextVars...>> {
    std::tuple<std::vector<std::pair<entt::entity, Components>>...> components;
    std::tuple<std::optional<ContextVars>...> context_vars;
};

template <typename... Components, typename... ContextVars, typename Archive>
void serialize_registry(entt::registry const& registry,
                        type_list<Components...>, type_list<ContextVars...>,
                        Archive& archive) {
    RegistryArchive<type_list<Components...>, type_list<ContextVars...>>
        snapshot;

    registry.each([&](entt::entity ent) {
        (
            [&]() {
                if (registry.any_of<Components>(ent)) {
                    if constexpr (std::is_empty_v<Components>) {
                        std::get<
                            std::vector<std::pair<entt::entity, Components>>>(
                            snapshot.components)
                            .emplace_back(ent, Components{});
                    } else {
                        std::get<
                            std::vector<std::pair<entt::entity, Components>>>(
                            snapshot.components)
                            .emplace_back(ent, registry.get<Components>(ent));
                    }
                }
            }(),
            ...);
    });

    (
        [&]() {
            if (auto ptr = registry.ctx().find<ContextVars>()) {
                std::get<std::optional<ContextVars>>(snapshot.context_vars)
                    .emplace(*ptr);
            }
        }(),
        ...);

    archive(snapshot.components, snapshot.context_vars);
}

template <typename T>
void set_context_var(entt::registry& registry, const std::optional<T>& ctx_var_opt) {
    if (ctx_var_opt.has_value()) {
        registry.ctx().emplace<T>(ctx_var_opt.value());
    }
}

template <typename... Components, typename... ContextVars, typename Archive>
void deserialize_registry(entt::registry& registry, type_list<Components...>, type_list<ContextVars...>, Archive& archive) {
    RegistryArchive<type_list<Components...>, type_list<ContextVars...>> snapshot;

    archive(snapshot.components, snapshot.context_vars);

    registry = entt::registry();

    (
        [&]() {
            for (const auto& [entity, component] : std::get<std::vector<std::pair<entt::entity, Components>>>(snapshot.components)) {
                if (!registry.valid(entity)) {
                    auto e = registry.create(entity);
                }
                registry.emplace_or_replace<Components>(entity, component);
            }
        }(),
        ...);

    (
        [&]() {
            set_context_var<ContextVars>(registry, std::get<std::optional<ContextVars>>(snapshot.context_vars));
        }(),
        ...);
}

struct position {
    float x, y, z;
};
struct velocity {
    float x, y, z;
};

template <class Archive>
void serialize(Archive& archive, position& v) {
    archive(v.x, v.y, v.z);
}

template <class Archive>
void serialize(Archive& archive, velocity& v) {
    archive(v.x, v.y, v.z);
}

template <class Archive>
void save(Archive& archive, entt::entity const& ent) {
    archive(static_cast<entt::id_type>(ent));
}

template <class Archive>
void load(Archive& archive, entt::entity& ent) {
    entt::id_type e;
    archive(e);
    ent = static_cast<entt::entity>(e);
}

int main() {
    entt::registry registry;
    auto e1 = registry.create();
    registry.emplace<position>(e1, position{ 1, 2, 3 });
    registry.ctx().emplace<velocity>(velocity{ 0, 0 });

    {
        auto view = registry.view<position>();
        view.each([](position &p)
        {
            std::cout << "Entity Save Value: " + std::to_string(p.x) << std::endl;
        });
	    std::cout << "Context Save Value: " << std::to_string(registry.ctx().get<velocity>().x) << std::endl;
    }

    std::cout << std::endl;

    std::string savePath = "state.bin";
    {
        std::ofstream os(savePath, std::ios::binary);
        cereal::PortableBinaryOutputArchive archive(os);
        serialize_registry(registry, type_list<position, velocity>{}, type_list<position, velocity>{}, archive);
    }

    {
        auto view = registry.view<position>();
        view.each([](position &p)
        {
            p.x += 99;
            std::cout << "Entity value changed to: " + std::to_string(p.x) << std::endl;
        });
	    registry.ctx().get<velocity>().x = 99;
	    std::cout << "Context value changed to: " << std::to_string(registry.ctx().get<velocity>().x) << std::endl;
    }

    std::cout << std::endl;

    {
    	std::cout << "load();" << std::endl;
        std::ifstream os(savePath, std::ios::binary);
        cereal::PortableBinaryInputArchive archive(os);
        deserialize_registry(registry, type_list<position, velocity>{}, type_list<position, velocity>{}, archive);
    }

    std::cout << std::endl;

    {
        auto view = registry.view<position>();
        view.each([](position &p)
        {
            std::cout << "After Load Entity (expected: 1): " << std::to_string(p.x) << std::endl;
        });

        std::cout << "After Load ctx (expected 0): " << std::to_string(registry.ctx().get<velocity>().x) << std::endl;
    }
}