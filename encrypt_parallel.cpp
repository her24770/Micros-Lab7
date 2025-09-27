/**
 * -----------------------------------------------------------
 * EncriptacionParalela.cpp
 * -----------------------------------------------------------
 * UNIVERSIDAD DEL VALLE DE GUATEMALA
 * Facultad de Ingeniería
 * Departamento de Ciencia de la Computación
 *
 * Descripción:
 * Implementa encriptación/desencriptación paralela usando
 * pthreads y OpenSSL AES-256-CBC con sincronización segura
 * -----------------------------------------------------------
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <pthread.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

using namespace std;
using namespace chrono;

// Constantes
const int BLOCK_SIZE = 1024 * 1024; // 1MB por bloque
const int KEY_SIZE = 32; // 256 bits
const int IV_SIZE = 16; // 128 bits

// Estructura para datos compartidos entre hilos
struct ThreadData {
    int thread_id;
    unsigned char* input_data;
    unsigned char* output_data;
    int data_size;
    unsigned char* key;
    unsigned char* iv;
    bool is_encrypt;
    int* output_size;
    bool success;
};

// Estructura para control de escritura ordenada
struct WriteControl {
    int next_block_to_write;
    pthread_mutex_t write_mutex;
    pthread_cond_t write_cond;
    ofstream* output_file;
    vector<pair<unsigned char*, int>>* completed_blocks;
    int total_blocks;
};

// Variables globales para sincronización
WriteControl write_control;

/**
 * Función para mostrar errores de OpenSSL
 */
void handleOpenSSLErrors() {
    ERR_print_errors_fp(stderr);
}

/**
 * Función para encriptar un bloque de datos
 */
bool encryptBlock(unsigned char* input, int input_len, 
                  unsigned char* key, unsigned char* iv,
                  unsigned char* output, int* output_len) {
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLErrors();
        return false;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        handleOpenSSLErrors();
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    int len;
    *output_len = 0;

    if (EVP_EncryptUpdate(ctx, output, &len, input, input_len) != 1) {
        handleOpenSSLErrors();
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    *output_len = len;

    if (EVP_EncryptFinal_ex(ctx, output + len, &len) != 1) {
        handleOpenSSLErrors();
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    *output_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return true;
}

/**
 * Función para desencriptar un bloque de datos
 */
bool decryptBlock(unsigned char* input, int input_len,
                  unsigned char* key, unsigned char* iv,
                  unsigned char* output, int* output_len) {
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLErrors();
        return false;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        handleOpenSSLErrors();
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    int len;
    *output_len = 0;

    if (EVP_DecryptUpdate(ctx, output, &len, input, input_len) != 1) {
        handleOpenSSLErrors();
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    *output_len = len;

    if (EVP_DecryptFinal_ex(ctx, output + len, &len) != 1) {
        handleOpenSSLErrors();
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    *output_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return true;
}

/**
 * Función ejecutada por cada hilo de trabajo
 */
void* workerThread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    // Buffer para datos procesados (con espacio extra para padding)
    unsigned char* processed_data = new unsigned char[data->data_size + EVP_MAX_BLOCK_LENGTH];
    int processed_size;
    
    // Procesar el bloque según la operación solicitada
    bool success;
    if (data->is_encrypt) {
        success = encryptBlock(data->input_data, data->data_size,
                              data->key, data->iv,
                              processed_data, &processed_size);
    } else {
        success = decryptBlock(data->input_data, data->data_size,
                              data->key, data->iv,
                              processed_data, &processed_size);
    }
    
    data->success = success;
    
    if (success) {
        // SECCIÓN CRÍTICA: Escritura ordenada al archivo
        pthread_mutex_lock(&write_control.write_mutex);
        
        // Guardar el bloque procesado en el vector de bloques completados
        write_control.completed_blocks->at(data->thread_id) = 
            make_pair(processed_data, processed_size);
        
        // Escribir bloques en orden si es posible
        while (write_control.next_block_to_write < write_control.total_blocks &&
               write_control.completed_blocks->at(write_control.next_block_to_write).first != nullptr) {
            
            int block_idx = write_control.next_block_to_write;
            auto& block_data = write_control.completed_blocks->at(block_idx);
            
            // Escribir bloque al archivo
            write_control.output_file->write(
                (char*)block_data.first, block_data.second);
            
            write_control.next_block_to_write++;
        }
        
        // Señalar que se completó el procesamiento
        pthread_cond_broadcast(&write_control.write_cond);
        pthread_mutex_unlock(&write_control.write_mutex);
        
    } else {
        delete[] processed_data;
    }
    
    return nullptr;
}

/**
 * Función para procesar archivo de forma paralela
 */
bool processFileParallel(const string& inputFile, const string& outputFile,
                        unsigned char* key, unsigned char* iv,
                        int num_threads, bool is_encrypt,
                        double& processing_time) {
    
    auto start_time = high_resolution_clock::now();
    
    // Leer archivo completo
    ifstream inFile(inputFile, ios::binary);
    if (!inFile) {
        cerr << "Error: No se pudo abrir " << inputFile << endl;
        return false;
    }
    
    // Obtener tamaño del archivo
    inFile.seekg(0, ios::end);
    size_t file_size = inFile.tellg();
    inFile.seekg(0, ios::beg);
    
    if (file_size == 0) {
        cerr << "Error: Archivo vacío" << endl;
        return false;
    }
    
    // Leer todos los datos
    vector<unsigned char> file_data(file_size);
    inFile.read((char*)file_data.data(), file_size);
    inFile.close();
    
    // Calcular número de bloques
    int total_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int actual_threads = min(num_threads, total_blocks);
    
    cout << "Procesando archivo de " << file_size << " bytes" << endl;
    cout << "Dividido en " << total_blocks << " bloques de " << BLOCK_SIZE << " bytes" << endl;
    cout << "Usando " << actual_threads << " hilos" << endl;
    
    // Abrir archivo de salida
    ofstream outFile(outputFile, ios::binary);
    if (!outFile) {
        cerr << "Error: No se pudo crear " << outputFile << endl;
        return false;
    }
    
    // Escribir IV al inicio del archivo encriptado
    if (is_encrypt) {
        outFile.write((char*)iv, IV_SIZE);
    }
    
    // Inicializar estructura de control de escritura
    write_control.next_block_to_write = 0;
    write_control.output_file = &outFile;
    write_control.total_blocks = total_blocks;
    write_control.completed_blocks = new vector<pair<unsigned char*, int>>(total_blocks);
    
    // Inicializar mutex y variable de condición
    pthread_mutex_init(&write_control.write_mutex, nullptr);
    pthread_cond_init(&write_control.write_cond, nullptr);
    
    // Crear datos para hilos
    vector<ThreadData> thread_data(actual_threads);
    vector<pthread_t> threads(actual_threads);
    
    // Procesar bloques con hilos
    int current_block = 0;
    bool all_success = true;
    
    while (current_block < total_blocks && all_success) {
        // Crear hilos para procesar bloques
        for (int t = 0; t < actual_threads && current_block < total_blocks; t++) {
            int block_start = current_block * BLOCK_SIZE;
            int block_size = min(BLOCK_SIZE, (int)(file_size - block_start));
            
            thread_data[t].thread_id = current_block;
            thread_data[t].input_data = file_data.data() + block_start;
            thread_data[t].data_size = block_size;
            thread_data[t].key = key;
            thread_data[t].iv = iv;
            thread_data[t].is_encrypt = is_encrypt;
            thread_data[t].success = false;
            
            if (pthread_create(&threads[t], nullptr, workerThread, &thread_data[t]) != 0) {
                cerr << "Error creando hilo " << t << endl;
                all_success = false;
                break;
            }
            
            current_block++;
        }
        
        // Esperar a que terminen los hilos
        for (int t = 0; t < actual_threads && t < current_block; t++) {
            pthread_join(threads[t], nullptr);
            if (!thread_data[t].success) {
                all_success = false;
            }
        }
    }
    
    // Limpiar memoria de bloques completados
    for (auto& block : *write_control.completed_blocks) {
        if (block.first != nullptr) {
            delete[] block.first;
        }
    }
    delete write_control.completed_blocks;
    
    // Destruir mutex y variable de condición
    pthread_mutex_destroy(&write_control.write_mutex);
    pthread_cond_destroy(&write_control.write_cond);
    
    outFile.close();
    
    auto end_time = high_resolution_clock::now();
    processing_time = duration<double>(end_time - start_time).count();
    
    return all_success;
}

/**
 * Función para procesar archivo de forma secuencial (para comparación)
 */
bool processFileSequential(const string& inputFile, const string& outputFile,
                          unsigned char* key, unsigned char* iv,
                          bool is_encrypt, double& processing_time) {
    
    auto start_time = high_resolution_clock::now();
    
    ifstream inFile(inputFile, ios::binary);
    if (!inFile) {
        cerr << "Error: No se pudo abrir " << inputFile << endl;
        return false;
    }
    
    // Leer archivo completo
    vector<unsigned char> buffer((istreambuf_iterator<char>(inFile)),
                                istreambuf_iterator<char>());
    inFile.close();
    
    if (buffer.empty()) {
        cerr << "Error: Archivo vacío" << endl;
        return false;
    }
    
    // Procesar datos
    vector<unsigned char> processed_data(buffer.size() + EVP_MAX_BLOCK_LENGTH);
    int processed_size;
    bool success;
    
    if (is_encrypt) {
        success = encryptBlock(buffer.data(), buffer.size(),
                              key, iv, processed_data.data(), &processed_size);
    } else {
        success = decryptBlock(buffer.data(), buffer.size(),
                              key, iv, processed_data.data(), &processed_size);
    }
    
    if (!success) {
        cerr << "Error en el procesamiento secuencial" << endl;
        return false;
    }
    
    // Guardar resultado
    ofstream outFile(outputFile, ios::binary);
    if (!outFile) {
        cerr << "Error: No se pudo crear " << outputFile << endl;
        return false;
    }
    
    if (is_encrypt) {
        outFile.write((char*)iv, IV_SIZE);
    }
    outFile.write((char*)processed_data.data(), processed_size);
    outFile.close();
    
    auto end_time = high_resolution_clock::now();
    processing_time = duration<double>(end_time - start_time).count();
    
    return true;
}

/**
 * Función para generar clave e IV aleatorios
 */
bool generateKeyAndIV(unsigned char* key, unsigned char* iv) {
    if (!RAND_bytes(key, KEY_SIZE) || !RAND_bytes(iv, IV_SIZE)) {
        cerr << "Error generando clave o IV aleatorios" << endl;
        return false;
    }
    return true;
}

/**
 * Función para leer IV del archivo encriptado
 */
bool readIVFromFile(const string& filename, unsigned char* iv) {
    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "Error: No se pudo abrir " << filename << endl;
        return false;
    }
    
    file.read((char*)iv, IV_SIZE);
    file.close();
    return true;
}

/**
 * Función para mostrar clave en formato hexadecimal
 */
void printKey(unsigned char* key, int size) {
    for (int i = 0; i < size; i++) {
        printf("%02x", key[i]);
    }
    cout << endl;
}

/**
 * Función para convertir string hexadecimal a bytes
 */
bool hexStringToBytes(const string& hex, unsigned char* bytes, int size) {
    if (hex.length() != size * 2) {
        return false;
    }
    
    for (int i = 0; i < size; i++) {
        sscanf(hex.substr(i * 2, 2).c_str(), "%2hhx", &bytes[i]);
    }
    return true;
}

/**
 * Función principal
 */
int main() {
    int option, num_threads;
    string inputFile, outputFile, keyHex;
    unsigned char key[KEY_SIZE], iv[IV_SIZE];
    
    cout << "=== ENCRIPTACIÓN PARALELA CON PTHREADS Y OPENSSL ===" << endl;
    cout << "1. Encriptar archivo" << endl;
    cout << "2. Desencriptar archivo" << endl;
    cout << "Seleccione una opción: ";
    cin >> option;
    
    cout << "Ingrese el nombre del archivo de entrada: ";
    cin >> inputFile;
    
    cout << "Ingrese el nombre del archivo de salida: ";
    cin >> outputFile;
    
    cout << "Ingrese el número de hilos (1-16): ";
    cin >> num_threads;
    num_threads = max(1, min(16, num_threads));
    
    double seq_time, par_time;
    bool success = false;
    
    if (option == 1) {
        // ENCRIPTACIÓN
        cout << "\n=== INICIANDO ENCRIPTACIÓN ===" << endl;
        
        // Generar clave e IV
        if (!generateKeyAndIV(key, iv)) {
            return 1;
        }
        
        // Mostrar clave generada
        cout << "Clave generada (guárdela para desencriptar): ";
        printKey(key, KEY_SIZE);
        
        // Procesamiento paralelo
        cout << "\n--- Encriptación Paralela (" << num_threads << " hilos) ---" << endl;
        success = processFileParallel(inputFile, outputFile, key, iv, num_threads, true, par_time);
        
        if (success) {
            cout << "✓ Encriptación paralela completada en: " << par_time << " segundos" << endl;
            
            // Comparación con versión secuencial
            cout << "\n--- Encriptación Secuencial (comparación) ---" << endl;
            string seq_output = outputFile + ".seq";
            if (processFileSequential(inputFile, seq_output, key, iv, true, seq_time)) {
                cout << "✓ Encriptación secuencial completada en: " << seq_time << " segundos" << endl;
                cout << "\n=== RESULTADOS DE RENDIMIENTO ===" << endl;
                cout << "Tiempo secuencial: " << seq_time << "s" << endl;
                cout << "Tiempo paralelo:   " << par_time << "s" << endl;
                cout << "Aceleración:       " << (seq_time / par_time) << "x" << endl;
                cout << "Eficiencia:        " << (seq_time / par_time / num_threads) * 100 << "%" << endl;
            }
        }
        
    } else if (option == 2) {
        // DESENCRIPTACIÓN
        cout << "\n=== INICIANDO DESENCRIPTACIÓN ===" << endl;
        
        // Leer IV del archivo
        if (!readIVFromFile(inputFile, iv)) {
            return 1;
        }
        
        // Solicitar clave
        cout << "Ingrese la clave en formato hexadecimal: ";
        cin >> keyHex;
        
        if (!hexStringToBytes(keyHex, key, KEY_SIZE)) {
            cerr << "Error: Clave inválida. Debe tener " << (KEY_SIZE * 2) << " caracteres hexadecimales." << endl;
            return 1;
        }
        
        // Procesamiento paralelo
        cout << "\n--- Desencriptación Paralela (" << num_threads << " hilos) ---" << endl;
        success = processFileParallel(inputFile, outputFile, key, iv, num_threads, false, par_time);
        
        if (success) {
            cout << "✓ Desencriptación paralela completada en: " << par_time << " segundos" << endl;
            
            // Comparación con versión secuencial
            cout << "\n--- Desencriptación Secuencial (comparación) ---" << endl;
            string seq_output = outputFile + ".seq";
            if (processFileSequential(inputFile, seq_output, key, iv, false, seq_time)) {
                cout << "✓ Desencriptación secuencial completada en: " << seq_time << " segundos" << endl;
                cout << "\n=== RESULTADOS DE RENDIMIENTO ===" << endl;
                cout << "Tiempo secuencial: " << seq_time << "s" << endl;
                cout << "Tiempo paralelo:   " << par_time << "s" << endl;
                cout << "Aceleración:       " << (seq_time / par_time) << "x" << endl;
                cout << "Eficiencia:        " << (seq_time / par_time / num_threads) * 100 << "%" << endl;
            }
        }
        
    } else {
        cerr << "Opción inválida" << endl;
        return 1;
    }
    
    if (!success) {
        cerr << "Error en el procesamiento" << endl;
        return 1;
    }
    
    return 0;
}