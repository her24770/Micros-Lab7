# Micros-Lab7
Este programa implementa encriptación/desencriptación paralela utilizando el algoritmo AES-256-CBC de OpenSSL con hilos POSIX (pthreads) para mejorar el rendimiento en archivos grandes.
Características Principales

-  Encriptación AES-256-CBC: Utiliza OpenSSL para encriptación segura
- Procesamiento Paralelo: Divide archivos en bloques de 1MB procesados por múltiples hilos
- Sincronización Segura: Implementa mutex y variables de condición para evitar condiciones de carrera
- Escritura Ordenada: Garantiza que los bloques se escriban en el orden correcto
- Comparación de Rendimiento: Mide y compara tiempos secuenciales vs. paralelos
-  Verificación de Integridad: Permite verificar que el proceso de encriptación/desencriptación sea correcto