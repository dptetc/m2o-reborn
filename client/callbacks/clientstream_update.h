/**
 * Entity remove from streamer
 */
void clientstream_update(librg::events::event_t* evt)
{
    auto event = (librg::events::event_bs_entity_t*) evt;
    auto game_entity    = event->entity.component<gamedata_t>();
    auto transform      = event->entity.component<librg::transform_t>();

    switch (event->type)
    {
        case TYPE_PLAYER:
        case TYPE_VEHICLE: {
            float x, y, z, w;
            // Vector3 position = ENTITY::GET_ENTITY_COORDS(game_entity->object, true);
            // ENTITY::GET_ENTITY_QUATERNION(game_entity->object, &x, &y, &z, &w);

            // transform->position = HMM_Vec3(
            //     position.x,
            //     position.y,
            //     position.z
            // );

            // transform->rotation = HMM_Vec4(x, y, z, w);

            // // write custom stuff
            // event->data->write_float(ENTITY::GET_ENTITY_SPEED(game_entity->object));
        }
    }
}
