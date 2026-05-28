import os
import shutil
Import("env")

def personal_release_pipeline(source, target, env):
    # 1. Récupérer le nom de l'environnement (ex: MKS_TFT28_V4_0)
    env_name = env.get("PIOENV")
    
    # 2. Définir le chemin cible : out/MKS_TFT28_V4_0/release/
    release_dir = os.path.join("out", env_name, "release")
    release_pic_dir = os.path.join(release_dir, "pic")
    release_sound_dir = os.path.join(release_dir, "sound")
    
    # Création de l'arborescence de sortie si elle n'existe pas
    for folder in [release_dir, release_pic_dir, release_sound_dir]:
        if not os.path.exists(folder):
            os.makedirs(folder)
            print(f"--> Création du dossier de release : {folder}")

    # 3. Récupérer et copier le fichier binaire (.bin) généré par PlatformIO
    compiled_bin = str(target[0]) 
    final_bin_name = f"MKSTFT28_{env_name}.bin"
    destination_bin = os.path.join(release_dir, final_bin_name)

    shutil.copyfile(compiled_bin, destination_bin)
    print(f"--> Firmware copié avec succès dans : {destination_bin}")

    # 4. Gestion dynamique des dossiers de ressources (res/pic et res/sound)
    # On définit les paires (Dossier Source, Dossier Destination)
    assets_mapping = {
        os.path.join("res", "pic"): release_pic_dir,
        os.path.join("res", "sound"): release_sound_dir
    }

    for src_dir, dest_dir in assets_mapping.items():
        if os.path.exists(src_dir):
            print(f"--> Détection des ressources dans {src_dir}, copie en cours...")
            for item in os.listdir(src_dir):
                s = os.path.join(src_dir, item)
                d = os.path.join(dest_dir, item)
                if os.path.isdir(s):
                    if os.path.exists(d): 
                        shutil.rmtree(d)
                    shutil.copytree(s, d)
                else:
                    shutil.copy2(s, d)
        else:
            print(f"--> Note : Le dossier source '{src_dir}' n'existe pas encore. Cible ignorée.")

    print("--> Pipeline de déploiement terminé avec succès !")

# Liaison du script à la fin de la génération du binaire par PlatformIO
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", personal_release_pipeline)