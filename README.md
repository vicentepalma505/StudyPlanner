# StudyPlanner
Mi primer projecto "Vibe coded" (Spec driven development + AI), implementando un generador de Mallas Curriculares universitarias.

![[SPmain.gif]]

##### WARNING: Este proyecto fue desarrollado con Inteligencia Artificial, y con el lenguaje de programación C, es decir, una receta para el desastre. Si quieres proteger tu preciada RAM, recomiendo encarecidamente que ejecutes esto en una Maquina Virtual. Si decides lo contrario, debe ser bajo tu propio riesgo.

##### WARNING: Al momento de publicación de este repositorio, esta aplicación solo funciona para distribuciones de Linux, No Windows.


## El porqué
Esta aplicación de escritorio nace de dos preguntas:
- ¿Se podría facilitar la visualización de mi plan de estudios?
- ¿Puedo hacer una aplicación de escritorio... con una librería para videojuegos?

(Para ser justos, esa última pregunta fue inspirada por el creador [@codingwithsphere](https://www.youtube.com/@codingwithsphere) y su [video](https://www.youtube.com/watch?v=KSKzaeZJlqk) explorando la misma idea. Un agradecimiento a el y su explicación.)

Para poner a prueba estas ideas, decidí usar la librería [Raylib](https://github.com/raysan5/raylib) (en particular, [Raygui](https://github.com/raysan5/raygui)), del desarrollador y profesor [Ramón Santamaria](https://github.com/raysan5), y el lenguaje de programación C. Mis agradecimientos incondicionales a el y su equipo por crear semejante herramienta para facilitar el desarrollo de videojuegos (y en este caso, Desktop apps) usando uno de los lenguajes más traicioneros del planeta.

## ¿Vibe-coded?
Este es el primer proyecto que realizo (casi) en su totalidad con inteligencia artificial. En particular, utilicé los modelos de [Opencode](https://opencode.ai/), principalmente por ser gratis. Pero, en vez de hacer "Vibe coding", que implicaría hacer idas y vueltas con la IA hasta crear la aplicación que quiero, decidí aplicar una técnica llamada Spec Driven Development (SDD), inspirado por el creador [@HolaMundoDev](https://www.youtube.com/@HolaMundoDev) y su [video](https://www.youtube.com/watch?v=8OjVlmrtW4M&t=2s) donde explica como hacer esto con IA. 

Básicamente, se trata de un proceso de descripción extenso de la aplicación a desarrollar, declarando cada proceso y subproceso, historias felices y tristes, etc (en metodo cascada, esto se consideraría "Requerimientos funcionales"). Al terminar estas especificaciones para cada funcionalidad, se implementa cada una de forma incremental, hasta conseguir la aplicación terminada. 

Esto minimiza la necesidad de corregir errores generados por la IA causados por la propia falta de explicación, dado que todo lo que la IA constructora debe saber, se encuentra en las specs previamente creadas. Por lo tanto, es más económico en cuanto a Tokens e iteraciones.

Al final de este README, puedes encontrar más información sobre el desarrollo de esta aplicación en particular.

## Como instalar
La compilación del source code en este proyecto genera un ejecutable 100% portable. Solo debes abrir el directorio donde descargaste el codigo fuente y ejecutar el Makefile adjunto.

1. Descargar el código fuente. Puedes descargar y descomprimir el Zip desde github, o clonarlo en tu máquina usando:
``` bash
git clone https://github.com/vicentepalma505/StudyPlanner.git
```

2. Dirigete al directorio fuente y ejecuta el comando make:
``` bash
	cd src
	make
```

3. El ejecutable 'StudyPlanner' debería haberse creado en el directorio. ¡Ahora puedes ubicarlo donde te plazca y empezar a utilizarlo!

## Como usar
Study Planner funciona sobre una cuadrícula. Los elementos principales son:

- Ramos: Los ramos son asignaturas universitarias, con nombre, código, créditos (SCT), semestre y año recomendados, y horas semanales (además de color). 
	ej. Álgebra, Literatura universal, Anatomía.

- Áreas: Las áreas son "clasificaciones" de los ramos. Varios ramos pueden ser parte de un área. 
	ej. Ciencias de la computación, Lengua y Literatura, Ciencias biológicas.

- Conexiones: Muchas veces, las universidades ofrecen asignaturas que requieren haber cursado otras previamente, conocidas como 'prerrequisito'. Las conexiones sirven para indicar cuando un ramo esta asociado a otro en calidad de prerrequisito. 
	ej. Álgebra 1 -> Álgebra 2
A continuación (y en el inicio de este readme) se presenta un ejemplo de flujo de trabajo dentro de Study Planner:
![[SPmain.gif]]


### Controles
#### Abrir

![[SPctrlO.gif]]

![[SPabrir.gif]]

#### Guardar

![[SPsave.gif]]

![[SPctrlS.gif]]

#### Moverse en el Grid

![[SPcontrols.gif]]

#### Zoom

![[SPzoom.gif]]
#### Selección

![[SPselect.gif]]

![[SPctrlnarrow.gif]]

#### Conectar

![[SPconectar.gif]]

#### Deshacer

![[SPctrlZ.gif]]

#### Eliminar

![[SPsupr1.gif]]

![[SPsupr2.gif]]

#### Exportar imagen

![[SPexportar.gif]]

## Archivo de Demo

Si quieres experimentar con una Malla pre-fabricada, he dejado un archivo de prueba en assets/demo_files con algunos ramos y áreas creadas.
## Bugs

Al momento de subir este proyecto, me he encontrado con algunos bugs que suceden cada cierta cantidad de ejecuciones. Estos incluyen:

- "Artifacts" gráficos, donde la pantalla de la aplicación se pixela y se vuelve inutilizable. Hay que cerrar y volver a abrir la aplicación para volver a la normalidad.
- "Crashes" aleatorios, usualmente luego de presentarse el bug anterior. Al volver a abrir, funciona correctamente.
Existe una posibilidad de que estos errores se deban a algun error en el manejo de memoria. Si bien Raylib se encarga de minimizar este tipo de riesgos, aún se puede lograr con suficiente inaptitud (como la mía).

Es por lo anterior que recalco el riesgo de este proyecto, y que se recomiendo encarecidamente NO EJECUTAR EN TU MÁQUINA DIRECTAMENTE, si no usar una máquina virtual para evitar daños a tu memoria. Usar IA y lenguajes de bajo nivel siempre es un riesgo, y trabajaré en los próximos meses para corregir (a mano, sin IA) los Bugs mencionados.

## Colaboración

Si te interesa este proyecto, eres totalmente bienvenid@ a probarlo, experimentar con el, y hacer tus Pull Requests. Sería un placer tener Feedback de desarrollador@s más experimentad@s que yo.

Disfruta del proyecto, y nos vemos pronto! B)