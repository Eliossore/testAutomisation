const express = require('express');
const { exec } = require('child_process');
const cors = require('cors'); // Pour permettre les requ�tes depuis votre front-end
const fs = require('fs');
const path = require('path');

const app = express();
const port = 3030;
const appProcessPath = './Json/process.json';
const systemMonitoringPath = './Json/system_monitoring.json'

let processRunning = false; // Indique si le processus est en cours d'ex�cution
let configuratorRunning = false; // Indique si le configurator est en cours d'ex�cution

// Middleware
app.use(cors());
app.use(express.json());

// Changer le dir par le correcte
const absolutePath = path.resolve('./ecofloc-UI/src');
if (fs.existsSync(absolutePath)) {
  process.chdir(absolutePath); // Change le répertoire de travail
  console.log(`Répertoire de travail changé à : ${absolutePath}`);
} else {
  console.error(`Le répertoire ${absolutePath} n'existe pas.`);
}

// API pour lancer l'ex�cutable
app.post('/execute', (req, res) => {
    const exePath = `"${__dirname}\\Generator.exe"`; // Chemin absolu vers l'ex�cutable

    if (processRunning) {
        return res.status(400).json({ success: false, message: 'Process already running' });
    }

    try {
        // Lancer l'ex�cutable
        const command = `${exePath} ${systemMonitoringPath}`;
        const process = exec(command, (error, stdout, stderr) => {
            processRunning = false; // R�initialiser apr�s ex�cution
            if (error) {
                console.error(`Erreur d'ex�cution : ${error.message}`);
                return; // Pas de `res.json` ici car la r�ponse a d�j� �t� envoy�e
            }

            if (stderr) {
                console.warn(`Stderr : ${stderr}`);
                return; // Idem, g�rer sans r�ponse suppl�mentaire
            }

            console.log(`Stdout : ${stdout}`);
        });

        processRunning = true; // Indique que le processus a d�marr�
        console.log('Process launched');
    } catch (error) {
        console.error(`Erreur inattendue : ${error.message}`);
        processRunning = false; // Assurer que l'�tat est r�initialis�
        return res.status(500).json({ success: false, message: 'Erreur lors du lancement du processus.' });
    }
});

// API pour lancer le configurator
app.post('/configurator', (req, res) => {
    const exePath = `"${__dirname}\\..\\..\\EcoflocConfigurator.exe"`; // Chemin absolu vers le configurator

    if (configuratorRunning) {
        return res.status(400).json({ success: false, message: 'Configurator already running' });
    }

    try {
        const process = exec(exePath, (error, stdout, stderr) => {
            configuratorRunning = false; // R�initialiser apr�s ex�cution
            if (error) {
                console.error(`Erreur d'ex�cution : ${error.message}`);
                return;
            }
            if (stderr) {
                console.warn(`Stderr : ${stderr}`);
                return;
            }
            console.log(`Stdout : ${stdout}`);
        });

        configuratorRunning = true;
        console.log('Configurator started');
    } catch (error) {
        console.error(`Erreur inattendue : ${error.message}`);
        configuratorRunning = false;
        return res.status(500).json({ success: false, message: 'Erreur lors du lancement du configurator.' });
    }
});


// API pour arr�ter l'ex�cutable
app.post('/stop', (req, res) => {
    const exeName = 'Generator.exe'; // Nom du processus � tuer

    if (!processRunning) {
        return res.status(400).json({ success: false, message: 'No process running' });
    }

    try {
        // Utilisation de taskkill pour arr�ter le processus par nom
        exec(`taskkill /IM ${exeName} /F`, (error, stdout, stderr) => {
            if (error) {
                console.error(`Erreur lors de l'arr�t : ${error.message}`);
                return res.status(500).json({ success: false, message: `Error : ${error.message}` });
            }

            if (stderr) {
                console.warn(`Stderr : ${stderr}`);
                // Optionnel : envoyer un avertissement
            }

            console.log(`Process stopped : ${stdout}`);
            processRunning = false;
        });
    } catch (error) {
        console.error(`Erreur inattendue : ${error.message}`);
        processRunning = false;
        return res.status(500).json({ success: false, message: 'Error while attempting to stop the process.' });
    }
});

app.post('/changeListePidState', (req, res) => {
    const { liste, etat } = req.body; // Récupère les données envoyées
    console.log('taille liste reçue :', liste.length);
    console.log('etat :', etat);
    const data = JSON.parse(fs.readFileSync(appProcessPath, 'utf-8'));
    // Réponse au client
    data.forEach(process => {
        if (liste.includes(process.name)) {
            process.pids.forEach(pidInfo => {
                pidInfo.checked = etat;
            });
        }
    });
    // Sauvegarder les modifications dans le fichier JSON
    fs.writeFileSync(appProcessPath, JSON.stringify(data, null, 4), 'utf-8');
    
    console.log('Valeur modifiée avec succès.');
    res.json({ message: 'Liste reçue avec succès', receivedListe: liste });
});

app.post('/changePidState', (req, res) => {
    console.log('Répertoire courant :', process.cwd());
    const { nomProc, pidProc, etat } = req.body; // Extract nomProc and pidProc from the request body
    console.log('Received changePidState request');
    console.log('nomProc:', nomProc);
    console.log('pidProc:', pidProc);
    console.log('Etat:', etat)

    // Récupérer
    const data = JSON.parse(fs.readFileSync(appProcessPath, 'utf-8'));

    // Parcourir les processus pour modifier la valeur
    data.forEach(process => {
        if (process.name == nomProc) {
            process.pids.forEach(pidInfo => {
                if (pidInfo.numeroPid == pidProc) {
                    pidInfo.checked = etat;
                }
            });
        }
    });
    // Sauvegarder les modifications dans le fichier JSON
    fs.writeFileSync(appProcessPath, JSON.stringify(data, null, 4), 'utf-8');
    
    console.log('Valeur modifiée avec succès.');

    // Example response:
    res.json({
        success: true,
        message: 'Process state changed successfully!',
        nomProc,
        pidProc,
        etat
    });
});

// Route SSE
app.get('/events', (req, res) => {
    // Configuration des headers pour SSE
    res.setHeader('Content-Type', 'text/event-stream');
    res.setHeader('Cache-Control', 'no-cache');
    res.setHeader('Connection', 'keep-alive');

    console.log('Client SSE connecté');

    // Envoi d'un message initial
    res.write(`data: ${JSON.stringify({ message: 'Connexion établie' })}\n\n`);

    const filePath = './Json/process.json';

    // Surveiller le fichier JSON
    const watcher = fs.watch(filePath, (eventType) => {
        if (eventType === 'change') {
            console.log(`Fichier modifié: ${filePath}`);

            // Lire le contenu du fichier
            fs.readFile(filePath, 'utf8', (err, data) => {
                if (err) {
                    console.error('Erreur de lecture du fichier:', err);
                    return;
                }

                // Vérifier si le contenu est un JSON valide
                try {
                    const jsonData = JSON.parse(data); // Valider le JSON
                    res.write(`data: ${JSON.stringify(jsonData)}\n\n`); // Envoyer au client
                } catch (parseErr) {
                    console.error('Erreur de parsing JSON:', parseErr);
                    res.write(`data: ${JSON.stringify({ error: 'Invalid JSON format' })}\n\n`);
                }
            });
        }
    });

    // Nettoyer le watcher lorsque le client se déconnecte
    req.on('close', () => {
        console.log('Client SSE déconnecté');
        watcher.close();
    });
});

// API pour lancer le configurator
app.post('/update', (req, res) => {
    const exePath = `"${__dirname}\\PIDRecup.exe"`; // Chemin absolu vers le configurator

     try {
        const command = `${exePath} ${appProcessPath}`;
        const process = exec(command, (error, stdout, stderr) => {
            configuratorRunning = false; // R�initialiser apr�s ex�cution
            if (error) {
                console.error(`Erreur d'ex�cution : ${error.message}`);
                return;
            }
            if (stderr) {
                console.warn(`Stderr : ${stderr}`);
                return;
            }
            console.log(`Stdout : ${stdout}`);
        });

        
    } catch (error) {
        console.error(`Erreur inattendue : ${error.message}`);
        configuratorRunning = false;
        return res.status(500).json({ success: false, message: 'Erreur lors du lancement du configurator.' });
    }
});


// D�marrer le serveur
app.listen(port, () => {
    console.log(`Serveur en cours d'ex�cution sur http://localhost:${port}`);
});
