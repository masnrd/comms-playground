import asyncio
import mavsdk

SYS_ID = 255
URL = "udp://:14540"


async def send_commands(drone: mavsdk.System):
    """ Send commands to an armed drone. """
    pass


async def rtb(drone: mavsdk.System):
    """ Return to base. Used to guard against program errors in `send_commands()`. """
    print("RTB command dispatched.")
    await drone.action.return_to_launch()


async def main():
    drone = mavsdk.System(sysid=SYS_ID)

    # Connect to drone
    await drone.connect(system_address=URL)

    # Arm drone
    try:
        await drone.action.arm()
    except mavsdk.action.ActionError as e:
        print("Error arming drone: ", e)
        return

    # Give commands
    try:
        await send_commands(drone)
    except Exception as e:
        print("Fatal Error: ", e)
        await rtb(drone)
    except KeyboardInterrupt:
        print("KeyboardInterrupt")
        await rtb(drone)

if __name__ == "__main__":
    asyncio.run(main())
