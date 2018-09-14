Proceso MDJ (File System)

El proceso MDJ - File System será el proceso que interpretaremos como el acceso al File System de
nuestro sistema. Este proceso se encargará de gestionar las peticiones que realicen los Procesos CPU
sobre los archivos a través del Kernel. Para poder llevar adelante la gestión de las peticiones utilizará
un sistema de archivos basado en el FileSystem FIFA.

Funcionamiento
El Proceso MDJ será un proceso de tipo servidor, es decir, que estará a la espera de las conexiones
de peticiones del DAM, validando por medio de un Handshake del protocolo la operación a realizar.
Además, el File System deberá atender las peticiones de manera concurrente.
Finalmente, sobre cada operación realizada por el MDJ, se deberá aplicar un retardo antes de
retornar dicha operación.
