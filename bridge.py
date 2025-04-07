import ctypes

# Load the shared library
lib = ctypes.CDLL('./libengine.so')  # Or whatever you named it

# Set up argument and return types
lib.get_legal_moves_for_sunfish.argtypes = [ctypes.c_char_p, ctypes.c_int]
lib.get_legal_moves_for_sunfish.restype = ctypes.c_char_p
lib.free_string.argtypes = [ctypes.c_char_p]
lib.free_string.restype = None

def get_moves(sunfish_board_str, white_to_move):
    color_int = 0 if white_to_move else 1
    board_bytes = sunfish_board_str.encode('utf-8')

    # Call C++ function
    result = lib.get_legal_moves_for_sunfish(board_bytes, color_int)

    # Convert result to string
    move_str = ctypes.string_at(result).decode('utf-8')

    # Free memory allocated in C++
    lib.free_string(result)

    return move_str.split(';') if move_str else []
