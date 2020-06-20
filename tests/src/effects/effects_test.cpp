#include "../main.hpp"

TEST_CASE( "Effects get id (pass)", "[single-file]" ) {
  Effects effects;
  effects.loadFromPath("effects/effects_test");
  Effect *effect = effects.getByID(123);

  REQUIRE( effect->getId() == 123 );
}