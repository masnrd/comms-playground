import asyncio
import mavsdk
from mavsdk.mission import (MissionItem, MissionPlan)
from traceback import format_exc

SYS_ID = 255
URL = "udp://:14540"
DEFAULT_ALTITUDE = 10  # in metres
DEFAULT_SPEED = 25     # in m/s


def get_mission_item(lat: float, long: float,
                     rel_alt: float = DEFAULT_ALTITUDE,
                     speed: float = DEFAULT_SPEED, fly_through: bool = True,
                     camAction: MissionItem.CameraAction = MissionItem.CameraAction.NONE,
                     accept_radius: float = 0.5
                     ) -> MissionItem:
    """ Helper function to get MissionItem """
    return MissionItem(lat, long, rel_alt, speed, fly_through,
                       float('nan'), float('nan'), camAction,
                       float('nan'), float('nan'), accept_radius,
                       float('nan'), float('nan'))


def get_mission() -> MissionPlan:
    mission_items = []
    mission_items.append(get_mission_item(47.398039859999997, 8.5455725400000002))
    mission_items.append(get_mission_item(47.398036222362471, 8.5450146439425509))
    mission_items.append(get_mission_item(47.397825620791885, 8.5450092830163271))

    return MissionPlan(mission_items)


async def report_progress(drone: mavsdk.System):
    async for prog in drone.mission.mission_progress():
        print(f"Progress: {prog.current}/{prog.total}")


async def block_until_not_flying(drone: mavsdk.System):
    """ Blocks until the drone stops flying. """
    was_flying = False
    async for flying in drone.telemetry.in_air():
        if flying:
            was_flying = flying

        if was_flying and not flying:
            # Drone is no longer flying
            return


async def send_commands(drone: mavsdk.System):
    """ Send commands to an armed drone. """
    mission_report_task = asyncio.ensure_future(report_progress(drone))

    # Setup mission
    mission_plan = get_mission()
    await drone.mission.set_return_to_launch_after_mission(True)

    print("Uploading mission.")
    print(mission_plan)
    await drone.mission.upload_mission(mission_plan)
    print("Mission uploaded.")

    print("Takeoff.")
    await drone.action.set_takeoff_altitude(DEFAULT_ALTITUDE)
    await drone.action.takeoff()

    # Start mission
    print("Starting mission.")
    await drone.mission.start_mission()
    print("Mission started.")

    # Wait until mission done.
    await block_until_not_flying(drone)
    print("Mission completed.")

    # Shut down reporting task
    mission_report_task.cancel()
    try:
        await mission_report_task
    except asyncio.CancelledError:
        pass

    return


async def rtb(drone: mavsdk.System):
    """
    Return to base.
    Used to guard against program errors in `send_commands()`.
    """
    print("RTB command dispatched.")
    await drone.action.return_to_launch()


async def main():
    drone = mavsdk.System(sysid=SYS_ID)

    # Connect to drone
    await drone.connect(system_address=URL)

    # Arm drone
    print("Waiting to arm drone.")
    async for health in drone.telemetry.health():
        if health.is_armable and health.is_global_position_ok:
            try:
                await drone.action.arm()
            except mavsdk.action.ActionError:
                print("Error arming drone: ", format_exc())
                return
            break
    print("Drone armed.")

    # Report position
    pos = await anext(drone.telemetry.position())
    print("Drone Position: ", pos)

    # Give commands
    try:
        await send_commands(drone)
    except Exception:
        print("Fatal Error: ", format_exc())
        await rtb(drone)
    except KeyboardInterrupt:
        print("KeyboardInterrupt")
        await rtb(drone)

if __name__ == "__main__":
    asyncio.run(main())
