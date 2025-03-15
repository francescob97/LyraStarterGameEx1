# LyraStarterGameEx1

Responsive implementation, using client prediction and server-reconciliation of 4 movement abilities:
- Teleport 
- Rewind Time 
- Wall Run 
- Wall Jump

These have been implemented as special moves inside the CharacterMovementComponent (CMC)

## How to contribute

Fork the project, create a new branch from main, open the Editor, go to Edit > EditorPreferences, then on the left, click "Lyra Developer Settings".
On your right, go to Allow player bots to Attack and untick it to avoid being shot while testing.
Once you are done open a pull request in the main project.

To test multiplayer: On the editor on the left select from "Common Maps" the "L_ShooterGym" level and set the net mode to "Play As Listen Server"

## General procedure

Ability and bindings have been set up according to the gameplay ability system used in Lyra.
So, for example for the teleport, a GA_Teleport (Gameplay Ability with Widget), the associated widget W_TeleportCooldown, 
the GE_Teleport (Gameplay Effect) and the gameplay tag were created and linked all in the AbilitySet_ShooterHero Data Asset, 
InputData_Hero Data Asset, Input action and IMC KBM (Input Mapping Context Keyboard).
Finally the teleport part inside the custom character movement component, called ESLyraCharacterMovementComponent, have been coded.


### Overview of Movement Pipeline

Looking to the movement pipeline, on every Tick PerformMove function will be called, which will execute all the movement logic, 
using variables replicated between the server and client, called safe variables.
Then it will create a SafeMove using SetMoveFor to save that safe variables into the saved one.
Then will call CanCombineWith and see for identical moves into the pending list.
With GetCompressFlags will pack our variables to send to the server.
Then, when the server receive the move, it will extract the variables with UpdateFromCompressedFlags and perform the move the client performed,
and in this way they should be identical.

## Teleport

    The player is able to teleport 10 meters forward upon pressing the T key.

### Implementation

This is achieved starting from the TeleportPressed function, which is called by the GA_Teleport on the client when player wants to teleport.
This checks the teleport cooldown, and sets the Safe_bWantsToTeleport.
In this way, in UpdateCharacterStateBeforeMovement the character CanTeleport, PerformTeleport will be called.
This function is very basic, It just takes the forward vector and calls SafeMoveUpdatedComponent() which Calls MoveUpdatedComponent() 
from UMovementComponent. Sweep is enabled, so any slope will stop the teleport. To avoid that, velocity can be used instead.

### Showcase

Server view is on the left, Client on the other side.

| <img src="Documentation/Images/TeleportServer.gif" alt="TeleportServer" style="width:720px;height:405px;"> |
|:----------------------------------------------------------------------------------------------------------:|
|                           Server Teleport from Server and Client points of view                            |


| <img src="Documentation/Images/TeleportClient.gif" alt="TeleportClient" style="width:720px;height:405px;"> |
|:----------------------------------------------------------------------------------------------------------:|
|                           Client Teleport from Server and Client points of view                            |


### File used

- B_Hero_ShooterMannequin: Hero BP, now using EsLyraCharacterMovementComponent as CMC
- InputData_Hero
- IMC_Default_KBM
- AbilitySet_ShooterHero
- IA_Ability_Teleport
- GA_Hero_Teleport
- W_TeleportCooldown
- ShooterCoreTags
- GE_HeroTeleport_Cooldown
- MI_UI_Teleport_ ...
- EsLyraCharacterMovementComponent.cpp, .h : Custom Character Movement component used to implement all abilities


## Rewind Time

    This feature is similar to the Tracer ability in Overwatch. Player should be able to rewind his time without affecting others' gameplay.

### Implementation

According to Overwatch Wiki, the Tracer's recall ability says that Tracer rewinds herself to exactly three seconds, and that 
lasts 1.25 seconds with a cooldown of 12 seconds. Tracer bounds backward in time, returning her health, ammo and position 
on the map to precisely where they were a few seconds before.
I didn't implement the rotation and the reloading. I've also skipped the part that if there is a moving platform it will consider it on the rewinding.

The GA_RewindTime will call the RewindTime pressed, and at the end of the ability, it will heal the player to the max health he had in the recording window.

Safe_bIsRewinding: stores the information if we are rewinding in time
Safe_RewindingIndex: it is the shared safe index which establish the correct move to do in case of corrections (Desync).
This index is shared between client and server overriding FCharacterNetworkMoveDataContainer and FCharacterNetworkMoveData structs

Depending on the value Safe_bIsRewinding, in UpdateCharacterStateBeforeMovement  will be called CollectRewindData or else PerformRewindingTime.
In CollectRewindData location and the life are sampled. When the array is full, the recording window slides, removing first element
In PerformRewindingTime rewinding is done, using interpolation techniques to move the character across the sampled Locations in a 
certain amount of time, like 1.25 secs.

### Showcase

Server view is on the left, Client on the other side.

| <img src="Documentation/Images/RewindTimeServer.gif" alt="RewindTimeServer" style="width:720px;height:405px;">  |
|:-----------------------------------------------------------------------------------------------------------:|
| Server Rewind Time from Server and Client points of view. <br/>Server healing<br/>Pck Lag 500, PckJitter 30 |


| <img src="Documentation/Images/RewindTimeClient.gif" alt="RewindTimeClient" style="width:720px;height:405px;"> |
|:--------------------------------------------------------------------------------------------------------------:|
|            Client Rewind Time from Server and Client points of view. <br/>Pck Lag 500, PckJitter 30            |




### File used

- InputData_Hero
- IMC_Default_KBM
- AbilitySet_ShooterHero
- IA_Ability_RewindTime
- GA_RewindTime
- W_RewindTimeCooldown
- ShooterCoreTags
- GE_RewindTime_Cooldown
- MI_UI_RewindTime_ ...
- EsLyraCharacterMovementComponent.cpp, .h
- LyraCharacter .cpp, .h: to get Health Component
- GE_RewindTime_InstantHeal

## Wall Run

    This feature is similar to the Lucio ability in Overwatch. Player is able to run and jump on walls for a specific amount of time

### Implementation

Giving an overview, we can Jump on walls, if space is being pressed it will start wallrunning for tot seconds, if space is 
released we will walljump in a direction that is a mix of the velocity and the looking direction.
Infact, like Lucio we are able to wallrun backward, or turn while wallriding. 

This is just a custom movement mode, to it doesn't rely on some Gameplay Ability, but uses a custom enum of ECustomMovementMode and Safe_bWantsToWallRun.

CanAttemptJump have been overwritten to read the Jump input in air, to start the wall run or to late jump. 
It was a const function so Safe_bWantsToWallRun is mutable. Also CanJumpInternal_Implementation in the Lyra character 
have been overwritten to support LateJump, because it is like a common jump, executed with the DoJump function, but in midair.
Meanwhile the Walljump, obtained by SetJumpEnd function that is called by the GA_Hero_Jump when the jump is released, is just an applied force.

Every tick TryWallRun is called, in UpdateCharacterStateBeforeMovement, but it is only accessed when Safe_bWantsToWallRun, so Jump is being pressed.
Here traces are casted on bottom, left and right. If something is hit on the sides, then a wall is nearby, so the switch 
to our custom movement mode can begin Forward or backward movement is detected by a DotProduct with the velocity,

PhysWallRun is physics function that manages the movement. A SideVector is constructed orthogonal to the velocity through a 
CrossProduct because the player isn't the reference to take, cause he can rotate while WallRunning. 
Next some rays are casted to get the nearby surface and apply a velocity that is tangentially to it.

If we run out of time or stop hitting the wall, we move to falling movement mode and start the late jump timer, where we can jump with the DOJump function.

Character ABP has been modified to handle the wallrunning animation, using a FullBody linked animation layer in the WallRun state in Locomotion state machine.
The animation will be played in the sequence player inside ABP_ItemAnimLayersBase, respectively mirrored considering Wallrunning direction and side.
I made a simple wallrun animation using the built in control rig inside Unreal.

### Showcase

Server view is on the left, Client on the other side.

| <img src="Documentation/Images/WallRunServer.gif" alt="WallRunServer" style="width:720px;height:405px;"> |
|:--------------------------------------------------------------------------------------------------------------:|
|    Server WallRun from Server and Client points of view. <br/>Pck Lag 500, PckJitter 30     |


| <img src="Documentation/Images/WallRunClient.gif" alt="WallRunClient" style="width:720px;height:405px;"> |
|:--------------------------------------------------------------------------------------------------------:|
|          Client WallRun from Server and Client points of view. <br/>Pck Lag 500, PckJitter 30           |



### File used

- GA_Hero_Jump
- EsLyraCharacterMovementComponent.cpp, .h
- LyraCharacter .cpp, .h
- B_Hero_ShooterMannequin
- CR_Mannequin_Body_Take1
- LS_WallRun_Right
- MannequinMirrorDataTable
- MM_WallRun
- ABP_Mannequin_Base
- ABP_ItemAnimLayersBase
- ALI_ItemAnimLayers

## Wall Jump

    The player is able to perform a lateral jump while flying very close to a wall. This jump will push the player to the opposite direction of the wall, and a bit higher than before

### Implementation

Given the little difference with the wall jump implemented during the wall run, I considered this request solved by the previous solution.

## Reference links

Thanks to **_delgoodie_** with this awesome video series that helped me a lot studying the Character Movement:  https://www.youtube.com/watch?v=urkLwpnAjO0&list=PLXJlkahwiwPmeABEhjwIALvxRSZkzoQpk

Thanks to **_NanceDevDiaries_** that helped me understand and move through Lyra project and its Gameplay Ability System: https://www.youtube.com/watch?v=zeOm45iWc7M

Recall ability from Overwatch wiki: https://overwatch-archive.fandom.com/wiki/Recall

Wall Run ability from Overwatch: https://www.youtube.com/watch?v=YH0ePK5ia50
