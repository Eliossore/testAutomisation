const { exec } = require('child_process');
const readline = require('readline');

// Fonction pour exécuter une commande en tant que promesse
function runCommand(command, description) {
  return new Promise((resolve, reject) => {
    console.log(`Starting: ${description}`);
    const process = exec(command, (err, stdout, stderr) => {
      if (err) {
        console.error(`Error in ${description}:`, err.message);
        reject(err);
        return;
      }
      if (stdout) console.log(`${description} stdout: ${stdout}`);
      if (stderr) console.error(`${description} stderr: ${stderr}`);
      resolve();
    });

    process.stdout.on('data', (data) => {
      console.log(`[${description}] stdout: ${data.toString()}`);
    });

    process.stderr.on('data', (data) => {
      console.error(`[${description}] stderr: ${data.toString()}`);
    });
  });
}

// Fonction principale pour exécuter les étapes séquentiellement
async function main() {
  try {
    // Étape 1 : Installer / mettre à jour les packages
    await runCommand('npm ci --prefix ./ecofloc-UI', 'Install/Update Packages');

    // Étape 2 : Ouvrir le navigateur
    await runCommand('start http://localhost:5173/src/', 'Open Browser');

    // Étape 3 : Démarrer le serveur Node.js
    runCommand('node ./ecofloc-UI/src/server.cjs', 'Node.js Server');

    // Étape 4 : Démarrer le serveur Vite
    runCommand('npm run dev --prefix ./ecofloc-UI', 'Vite Development Server');


  } catch (err) {
    console.error('An error occurred during execution:', err.message);
  } finally {
    // Garder la fenêtre de terminal ouverte
    readline
      .createInterface({
        input: process.stdin,
        output: process.stdout,
      })
      .question('Press ENTER to exit...', () => {
        process.exit();
      });
  }
}

// Lancer le script principal
main();
