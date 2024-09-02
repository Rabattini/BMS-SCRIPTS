import tkinter as tk
from tkinter import filedialog
from tkinter import messagebox
import struct
import zlib

def read_long(file):
    return struct.unpack('<I', file.read(4))[0]

def write_long(file, value):
    file.write(struct.pack('<I', value))

def compress_chunk(data):
    try:
        compressed_data = zlib.compress(data)
        return compressed_data
    except zlib.error as e:
        print(f"Compression error: {e}")
        return None

def decompress_chunk(compressed_data):
    try:
        decompressed_data = zlib.decompress(compressed_data)
        return decompressed_data
    except zlib.error as e:
        print(f"Decompression error: {e}")
        return None

def compress_file(input_file_path, output_file_path):
    with open(input_file_path, 'rb') as input_file, open(output_file_path, 'wb') as output_file:
        input_file.seek(0, 2)
        total_length = input_file.tell()
        input_file.seek(0)

        write_long(output_file, total_length)

        while True:
            chunk = input_file.read(1024 * 1024)  # Read 1MB chunks
            if not chunk:
                break

            compressed_chunk = compress_chunk(chunk)
            if compressed_chunk:
                write_long(output_file, len(compressed_chunk))
                output_file.write(compressed_chunk)

def decompress_file(input_file_path, output_file_path):
    with open(input_file_path, 'rb') as input_file, open(output_file_path, 'wb') as output_file:
        total_length = read_long(input_file)

        offset = 4  # Start after the total_length
        while offset < total_length:
            input_file.seek(offset)
            chunk_length = read_long(input_file)

            if chunk_length > 0:
                compressed_chunk = input_file.read(chunk_length)
                decompressed_chunk = decompress_chunk(compressed_chunk)
                
                if decompressed_chunk:
                    output_file.write(decompressed_chunk)
                
                offset += 4 + chunk_length  # Move past the chunk_length and data
            else:
                break

def browse_input_file():
    filename = filedialog.askopenfilename(filetypes=[("Files", "*.*")])
    input_entry.delete(0, tk.END)
    input_entry.insert(0, filename)

def browse_output_file():
    filename = filedialog.asksaveasfilename(defaultextension="*.*", filetypes=[("Files", "*.*")])
    output_entry.delete(0, tk.END)
    output_entry.insert(0, filename)

def decompress_action():
    input_path = input_entry.get()
    output_path = output_entry.get()
    if input_path and output_path:
        decompress_file(input_path, output_path)
        messagebox.showinfo("Success", "File decompressed successfully!")
    else:
        messagebox.showwarning("Input Error", "Please select both input and output files.")

def compress_action():
    input_path = input_entry.get()
    output_path = output_entry.get()
    if input_path and output_path:
        compress_file(input_path, output_path)
        messagebox.showinfo("Success", "File compressed successfully!")
    else:
        messagebox.showwarning("Input Error", "Please select both input and output files.")

# GUI setup
root = tk.Tk()
root.title("MUA 3 - File Compressor/Decompressor - Made by Rabattini (Luke)")

tk.Label(root, text="Input File:").grid(row=0, column=0, padx=10, pady=5)
tk.Label(root, text="Output File:").grid(row=1, column=0, padx=10, pady=5)

input_entry = tk.Entry(root, width=60)
input_entry.grid(row=0, column=1, padx=10, pady=5)
tk.Button(root, text="Browse", command=browse_input_file).grid(row=0, column=2, padx=10, pady=5)

output_entry = tk.Entry(root, width=60)
output_entry.grid(row=1, column=1, padx=10, pady=5)
tk.Button(root, text="Browse", command=browse_output_file).grid(row=1, column=2, padx=10, pady=5)

tk.Button(root, text="Decompress", command=decompress_action).grid(row=2, column=1, padx=10, pady=10)
tk.Button(root, text="Compress", command=compress_action).grid(row=2, column=2, padx=10, pady=10)

root.mainloop()
