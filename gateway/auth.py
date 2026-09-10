def authenticate_device(device_id,token):
    """
    simple authentication machanism in a real scenario
    """
    allow_devices={
        "P001": "token123",
        "H001": "token456"
    }
    if device_id in allow_devices and allow_devices[device_id] == token:
        return True
    return False