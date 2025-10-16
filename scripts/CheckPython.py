import subprocess
import sys
import pkg_resources

def install(package):
    print(f"  Instalando {package}...")
    subprocess.check_call([sys.executable, '-m', 'pip', 'install', package, '--quiet'])
    print(f"  ✓ {package} instalado")

def ValidatePackage(package):
    required = {package}
    installed = {pkg.key for pkg in pkg_resources.working_set}
    missing = required - installed

    if missing:
        install(package)
    else:
        print(f"  ✓ {package} ya instalado")

def ValidatePackages():
    ValidatePackage('requests')
    ValidatePackage('fake-useragent')