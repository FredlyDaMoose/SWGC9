# SWGC

Developed with Unreal Engine 5

Demo: https://youtu.be/ZM2touwLIMg

For my project, I set out to create an interaction system for my Unreal Engine 4 fan/hobby game, however,
in the middle of the semester, Epic Games released Unreal Engine 5 https://www.unrealengine.com/en-US/blog/unreal-engine-5-is-now-available
, and alongside it, a new framework for online game development called Lyra https://docs.unrealengine.com/5.0/en-US/lyra-sample-game-in-unreal-engine/
which consists of many modular, industry-standard features. Needless to say, it has every feature I could
ever want to add to my game, and they were the all best practice- straight from the developers of the engine.  Upon its release, I decided
to shift my efforts to understanding Lyra, and combining it with Advanced Locomotion System (ALS) https://github.com/Sixze/ALS-Refactored
, which is "an advanced pipedal locomotion and layering system focusing on high quality character animation awith responsive movement".
Despite Lyra being an official release from Epic Games, I find ALS to be a much better movement system than the one Lyra implements. ALS is so
well-made and recognized, that its original developer was hired by Epic Games as an Animation Technical Designer. https://twitter.com/clongmire42?lang=en

The source code for the project is confusing and expansive, so I'll be focusing on a single class to highlight the various object-oriented design
concepts. That class is SwgcCharacter. Starting at the name, SWGC is the abbreviation of the project's name, "Star Wars: Galactic Conquest". As with the
rest of project, I closely follow Unreal's unofficial style guide: https://github.com/Allar/ue5-style-guide
That repository is a really useful source and, in my opinion, should be recognized officially by Epic Games, since there isn't an official style guide or naming
convention for the engine, and today's standards are a culmination of years of public use/development.
 
I won't get too into Unreal's class hierarchy, but the basics are

UObject - The base Unreal object class. Cannot be directly placed in a scene as it doesn't have location information. The "U" before its name and other UObject class decendents names signifies its UObject inheritance. This letter is dropped from instance names. While not technically the true base class (It inherits from UObjectBaseUtility which inherits from UObjectBase), it is the lowest level class that's not considered "low level" and is reguluarly used by developers.

AActor - A UObject that can be placed/spawned into a scene. The "A" before its name and other AActors replaces the "U" and signifies its AActor inheritance.

APawn - An AActor that can be controlled/possessed by players or AI.

ACharacter - An APawn that has mesh, collision, and built-in movement logic.

AAlsCharacter - ALS's expansion of ACharacter that contains all of its advanced movement and basic logic.

ASwgcCharacter - My expansion of AAlsCharacter that contains game-specific character logic that acts as the main bridge between the ALS plugin and the rest of the game's framework.



