import subprocess
import sys

try:
    from importlib.metadata import distributions
except ImportError:
    # Fallback for Python < 3.8
    from importlib_metadata import distributions

def install(package):
    print(f"  Instalando {package}...")
    subprocess.check_call([sys.executable, '-m', 'pip', 'install', package, '--quiet'])
    print(f"  ✓ {package} instalado")

def ValidatePackage(package):
    try:
        __import__(package.replace('-', '_'))
        print(f"  ✓ {package} ya instalado")
    except ImportError:
        install(package)

def ValidatePackages():
    ValidatePackage('requests')
    ValidatePackage('fake-useragent')