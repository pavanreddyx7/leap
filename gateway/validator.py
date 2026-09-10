def validate_telemetry(payload):
    """
    Validates incoming telemetry payload.
    expected format:
    {
      "device_id":"string",
      "type":"pole" | "home",
      "status":"ON" | "OFF",
      "tiemstamp": "iso8601 string or float",
      "voltage":float(otptional)
    }
    """
    if not isinstance(payload, dict):
        return False, "payload must be a JSON object"

    required_keys = ["device_id","type","status","timestamp"]
    for key in required_keys:
        if key not in payload:
            return False, f"Missing required key:{key}"

    if payload.get("type") not in ["pole","home"]:
        return False, "Invalid type, must be 'pole', or 'home' "

    if paylaod.get("status") not in ["ON","OFF"]:
        return False, "Invalid status, must be 'ON' or 'OFF' "

    return True, "Valid"