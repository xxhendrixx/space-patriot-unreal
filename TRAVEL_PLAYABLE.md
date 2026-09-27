# Playable Kestrel travel slice

`BP_KestrelFlyable` inherits `ASPFlightPawn`, which now owns the native travel authority and camera-local hyperdrive visuals. The saved `L_KestrelFlight` map supplies the generated Worldworks surface. Press **N** to cycle among the 19 source world IDs, **J** to begin charging, and **K** or **X** to cancel a charge. Retract gear with **G** and fly clear of the port: charge requires a powered, piloted ship, closed cargo hatch, at least 2 km above the generated surface, more than 5 km from the Kellen Reach origin, sufficient fuel, and acceptable heat.

Charge shows restrained alignment brackets. Paid transit freezes ordinary flight input and shows moving star streaks. After three seconds the ship is placed at an authored local exterior marker, the existing Worldworks renderer activates the destination's source fields, and only then does travel authority confirm the new world ID. The ship returns to SCM/manual flight with zero velocity and flight assist on. A failed world activation aborts transit safely without falsely recording arrival; spent jump fuel is not refunded.

This is a **local exterior arrival**, not a physically simulated interstellar route or a finished planet/city scene. The existing Earth port map remains a prototype environment. The destination's world seed and terrain/climate fields switch; authored city geometry, atmosphere, ocean, flora, fauna, orbital layouts, docking, and game saving are separate work.

Verification: UE 5.8 editor build; `SpacePatriot.Travel` automation suite including `PlayablePawnJourney`; `Tools/ValidateTravelPlayable.py` against the saved map and Blueprint.
