# Heap File Storage System in C

## Description

This project implements a heap file storage system using the C programming language.

It was developed as part of a university assignment and focuses on low-level file management, block-based storage, and record handling.

The system supports creating a heap file, opening and closing it, inserting records, and retrieving records by ID.

## Technologies Used

- C
- Block-based file storage
- File handling
- Data structures

## Features

- Create a heap file
- Open and close heap files
- Insert records into file blocks
- Store metadata for heap files and blocks
- Search and retrieve records by ID
- Use block-level file management through the BF library

## Project Structure

```text
heap-file-storage-c/
├── hp_file.c
├── hp_file.h
├── record.c
├── record.h
├── bf.h
├── bf_main.c
├── Makefile
└── README.md