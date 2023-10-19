""" 
# RISE.py
Simple script that makes the drone RISE and then land.
"""

import asyncio
import mavsdk
from math import isclose

URL = "udp://:14540"  # API-PX4 SITL Link
SYS_ID = 69           # System ID for this system
ALT = 10              # Default altitude


async def block_till_altitude(drone: mavsdk.System, tgt_alt: float, tol: float):
    """ Blocks till the drone is at relative altitude (tgt_alt +- tol) """
    async for pos in drone.telemetry.position():
        rel_alt = pos.relative_altitude_m

        if isclose(rel_alt, tgt_alt, abs_tol=tol):
            return


async def give_commands(drone: mavsdk.System):
    print("RISE MY CHILD")
    await drone.action.set_takeoff_altitude(ALT)
    await drone.action.takeoff()
    print("Takeoff command dispatched.")

    # Block until takeoff complete -- can't really detect if it's at the desired altitude otherwise.
    await block_till_altitude(drone, ALT, 1)
    print("Drone is at takeoff altitude.")

    print("land pls")
    await drone.action.land()
    print("Landing command dispatched.")

    # Block until landed
    await block_till_altitude(drone, 0, 1)
    print("Drone has landed.")


async def main():
    drone = mavsdk.System(sysid=SYS_ID)

    # Connect to drone
    print(f"Connecting to {URL}...")
    await drone.connect(system_address=URL)
    print("Connected!")

    # Arm drone
    try:
        print("Arming drone...")
        await drone.action.arm()
        print("Drone armed!")
    except mavsdk.action.ActionError as e:
        print("Error arming drone: ", e)
        return

    # Give commands
    try:
        await give_commands(drone)
    except Exception as e:
        # Generic error, tell drone to RTB
        print("ERROR: ", e)
        print("RTB instruction dispatched.")
        await drone.action.return_to_launch()
    except KeyboardInterrupt:
        # Tell drone to RTB
        print("Keyboard interrupt.")
        print("RTB instruction dispatched.")
        await drone.action.return_to_launch()

if __name__ == "__main__":
    asyncio.run(main())
