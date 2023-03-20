# entt-cereal-mvp
Proof of concept for serialization of entities + components + context with entt

This repository exists for reference for my future self. I couldn't find a complete or working example for saving / loading state with cereal in the latest version of entt.

The program creates an entity and creates a context and sets some state on both. It then saves the state, modifies the state, then loads the save file, printing each step along the way to verify the state is as expected.
