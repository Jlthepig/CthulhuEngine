#include <cmath>
#include <filesystem>

#include "components.hpp"
#include "engine.hpp"
#include "pathEncoding.hpp"
#include "session.hpp"

namespace Cthulhu::Scribe
{
namespace
{
class PlaceModelCommand final : public Command
{
  public:
    PlaceModelCommand(Engine &engine, Scene::EntityId id, Assets::AssetId asset, glm::vec3 position)
        : engine(engine),
          id(id),
          asset(asset),
          position(position)
    {}

    std::string_view name() const override
    {
        return "Place Model";
    }

    Result apply(Context &context) override
    {
        auto &assets = engine.getAssetManager();
        const auto *project = engine.getProject();
        const auto *record = assets.getRegistry().findById(asset);
        if (!project || !record || record->type != Assets::AssetType::Model || record->missing)
        {
            return Result::failed("MODEL ASSET IS UNAVAILABLE");
        }

        const auto file = project->resolveResourcePath(record->path);
        std::error_code error;
        if (!file || !std::filesystem::is_regular_file(*file, error))
        {
            return Result::failed("MODEL FILE IS UNAVAILABLE: " + record->path);
        }

        const Scene::MeshComponent mesh{record->path};
        if (!assets.loadModel(mesh.modelPath).isValid())
        {
            return Result::failed("COULD NOT LOAD MODEL: " + mesh.modelPath);
        }

        auto entity = context.scene.createEntityWithId(id, Utils::pathToUtf8(file->stem()));
        if (!entity)
        {
            return Result::failed("FAILED TO CREATE MODEL ENTITY");
        }

        entity->set(Scene::TransformComponent{.position = position});
        entity->set(mesh);
        context.emit(ChangeType::EntityCreated, id);
        return Result::applied();
    }

    Result revert(Context &context) override
    {
        if (!context.scene.destroyEntity(id))
        {
            return Result::failed("MODEL ENTITY DOES NOT EXIST");
        }

        context.emit(ChangeType::EntityDestroyed, id);
        return Result::applied();
    }

  private:
    Engine &engine;
    Scene::EntityId id;
    Assets::AssetId asset;
    glm::vec3 position;
};
} // namespace

EntityResult Session::placeModel(Assets::AssetId asset, glm::vec3 position)
{
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
    {
        return {Result::failed("INVALID MODEL POSITION"), {}};
    }

    auto *scene = syncScene();
    if (!scene)
    {
        return {Result::failed("NO ACTIVE SCENE"), {}};
    }

    auto id = Scene::generateEntityId();
    while (scene->isEntityAlive(id))
    {
        id = Scene::generateEntityId();
    }

    Result result = execute(std::make_unique<PlaceModelCommand>(engine, id, asset, position));
    return {result, result.status == Result::Status::Applied ? id : Scene::EntityId{}};
}
} // namespace Cthulhu::Scribe