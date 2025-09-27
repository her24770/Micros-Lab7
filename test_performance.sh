#!/bin/bash
# Script de pruebas de rendimiento para encriptación paralela
# Universidad del Valle de Guatemala

echo "=================================================="
echo "PRUEBAS DE RENDIMIENTO - ENCRIPTACIÓN PARALELA"
echo "=================================================="

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Verificar que el programa esté compilado
if [ ! -f "./encrypt_parallel" ]; then
    echo -e "${RED}Error: Programa no encontrado. Compilando...${NC}"
    make
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error en la compilación${NC}"
        exit 1
    fi
fi

# Crear directorio de resultados
mkdir -p test_results
cd test_results

# Generar archivo de prueba grande si no existe
if [ ! -f "large_test.txt" ]; then
    echo -e "${YELLOW}Generando archivo de prueba grande...${NC}"
    # Crear archivo de ~10MB
    for i in {1..100000}; do
        echo "Esta es la línea $i del archivo de prueba para encriptación paralela. Universidad del Valle de Guatemala - Facultad de Ingeniería - Departamento de Ciencia de la Computación."
    done > large_test.txt
    echo -e "${GREEN}Archivo large_test.txt creado ($(du -h large_test.txt | cut -f1))${NC}"
fi

# Función para ejecutar prueba de encriptación
test_encrypt() {
    local threads=$1
    local input_file=$2
    local output_file="encrypted_${threads}t.enc"
    
    echo -e "${YELLOW}Probando encriptación con $threads hilo(s)...${NC}"
    
    # Crear archivo de comandos temporales
    echo -e "1\n$input_file\n$output_file\n$threads" > temp_commands.txt
    
    # Ejecutar programa y capturar la clave
    ../encrypt_parallel < temp_commands.txt > output_${threads}t.txt 2>&1
    
    # Extraer información de tiempo y clave
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Encriptación con $threads hilo(s) completada${NC}"
        grep "completada en:" output_${threads}t.txt
        grep "Aceleración:" output_${threads}t.txt
        grep "Eficiencia:" output_${threads}t.txt
        
        # Guardar la clave
        grep "Clave generada" output_${threads}t.txt | cut -d: -f2 | tr -d ' ' > key_${threads}t.txt
        
        echo "Archivo encriptado: $output_file ($(du -h $output_file 2>/dev/null | cut -f1))"
    else
        echo -e "${RED}✗ Error en encriptación con $threads hilo(s)${NC}"
        cat output_${threads}t.txt
    fi
    
    rm -f temp_commands.txt
    echo ""
}

# Función para ejecutar prueba de desencriptación
test_decrypt() {
    local threads=$1
    local encrypted_file="encrypted_${threads}t.enc"
    local decrypted_file="decrypted_${threads}t.txt"
    local key_file="key_${threads}t.txt"
    
    if [ ! -f "$encrypted_file" ] || [ ! -f "$key_file" ]; then
        echo -e "${RED}No se encontraron archivos necesarios para desencriptar con $threads hilo(s)${NC}"
        return 1
    fi
    
    echo -e "${YELLOW}Probando desencriptación con $threads hilo(s)...${NC}"
    
    local key=$(cat $key_file)
    echo -e "2\n$encrypted_file\n$decrypted_file\n$threads\n$key" > temp_commands.txt
    
    ../encrypt_parallel < temp_commands.txt > decrypt_output_${threads}t.txt 2>&1
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Desencriptación con $threads hilo(s) completada${NC}"
        grep "completada en:" decrypt_output_${threads}t.txt
        grep "Aceleración:" decrypt_output_${threads}t.txt
        grep "Eficiencia:" decrypt_output_${threads}t.txt
        
        echo "Archivo desencriptado: $decrypted_file ($(du -h $decrypted_file 2>/dev/null | cut -f1))"
    else
        echo -e "${RED}✗ Error en desencriptación con $threads hilo(s)${NC}"
        cat decrypt_output_${threads}t.txt
    fi
    
    rm -f temp_commands.txt
    echo ""
}

# Función para verificar integridad
verify_integrity() {
    local original="large_test.txt"
    local decrypted="decrypted_${1}t.txt"
    
    if [ -f "$decrypted" ]; then
        if cmp -s "$original" "$decrypted"; then
            echo -e "${GREEN}✓ Verificación de integridad EXITOSA para $1 hilo(s)${NC}"
        else
            echo -e "${RED}✗ Verificación de integridad FALLIDA para $1 hilo(s)${NC}"
        fi
    fi
}

echo "Archivo de prueba: large_test.txt ($(du -h large_test.txt | cut -f1))"
echo ""

# Ejecutar pruebas con diferentes números de hilos
thread_counts=(1 2 4 8)

echo "=== FASE 1: PRUEBAS DE ENCRIPTACIÓN ==="
for threads in "${thread_counts[@]}"; do
    test_encrypt $threads "large_test.txt"
done

echo "=== FASE 2: PRUEBAS DE DESENCRIPTACIÓN ==="
for threads in "${thread_counts[@]}"; do
    test_decrypt $threads
done

echo "=== FASE 3: VERIFICACIÓN DE INTEGRIDAD ==="
for threads in "${thread_counts[@]}"; do
    verify_integrity $threads
done

echo "=== RESUMEN DE ARCHIVOS GENERADOS ==="
ls -lh *.enc *.txt 2>/dev/null | head -20

echo ""
echo "=== ANÁLISIS DE RENDIMIENTO ==="
echo "Archivo de prueba: $(du -h large_test.txt | cut -f1)"
echo ""

# Extraer y mostrar tiempos de encriptación
echo "TIEMPOS DE ENCRIPTACIÓN:"
for threads in "${thread_counts[@]}"; do
    if [ -f "output_${threads}t.txt" ]; then
        time_parallel=$(grep "Encriptación paralela completada en:" "output_${threads}t.txt" | grep -o '[0-9.]*' | head -1)
        time_sequential=$(grep "Encriptación secuencial completada en:" "output_${threads}t.txt" | grep -o '[0-9.]*' | head -1)
        speedup=$(grep "Aceleración:" "output_${threads}t.txt" | grep -o '[0-9.]*' | head -1)
        efficiency=$(grep "Eficiencia:" "output_${threads}t.txt" | grep -o '[0-9.]*' | head -1)
        
        printf "%-2d hilo(s): Paralelo=%-8s Secuencial=%-8s Aceleración=%-6sx Eficiencia=%-6s%%\n" \
               $threads "${time_parallel}s" "${time_sequential}s" "$speedup" "$efficiency"
    fi
done

echo ""
echo "TIEMPOS DE DESENCRIPTACIÓN:"
for threads in "${thread_counts[@]}"; do
    if [ -f "decrypt_output_${threads}t.txt" ]; then
        time_parallel=$(grep "Desencriptación paralela completada en:" "decrypt_output_${threads}t.txt" | grep -o '[0-9.]*' | head -1)
        time_sequential=$(grep "Desencriptación secuencial completada en:" "decrypt_output_${threads}t.txt" | grep -o '[0-9.]*' | head -1)
        speedup=$(grep "Aceleración:" "decrypt_output_${threads}t.txt" | grep -o '[0-9.]*' | head -1)
        efficiency=$(grep "Eficiencia:" "decrypt_output_${threads}t.txt" | grep -o '[0-9.]*' | head -1)
        
        printf "%-2d hilo(s): Paralelo=%-8s Secuencial=%-8s Aceleración=%-6sx Eficiencia=%-6s%%\n" \
               $threads "${time_parallel}s" "${time_sequential}s" "$speedup" "$efficiency"
    fi
done

echo ""
echo -e "${GREEN}Pruebas completadas. Archivos guardados en directorio test_results/${NC}"
echo "Comandos útiles:"
echo "  - Ver todos los logs: cat test_results/*output*.txt"
echo "  - Comparar archivos: diff test_results/large_test.txt test_results/decrypted_*t.txt"
echo "  - Limpiar resultados: rm -rf test_results/"

cd ..