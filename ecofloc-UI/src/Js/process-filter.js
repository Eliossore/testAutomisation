const listeProcessusHtmlElement = document.getElementById("ListeProcessus");
const tableFilterHtmlElement = document.getElementById("TableFilter");
const checkBoxSelectAllProcElement = document.getElementById("SelectAllProc");

let mesProcessus = [];
let setCategorie = new Set();
function areSetsEqual(set1, set2) {
    if (set1.size !== set2.size) {
        return false; // Si les tailles sont différentes, les Set ne sont pas égaux
    }

    // Comparer chaque élément dans set1
    for (let item of set1) {
        if (!set2.has(item)) {
            return false; // Si un élément de set1 n'est pas dans set2, ils ne sont pas égaux
        }
    }

    return true; // Si toutes les conditions sont remplies, les Set sont égaux
}

function makeGroupApplication(){
    if(tableFilterHtmlElement) {
        tableFilterHtmlElement.innerHTML = '';
        for (const item of setCategorie) {
            const lineDiv = document.createElement('div');
            lineDiv.classList.add('line', 'col-3');

            const labelDiv = document.createElement('div');
            labelDiv.textContent = item;

            const input = document.createElement('input');
            input.type = 'checkbox';
            input.id = item+'Filter';
            input.value = item;
            input.checked = true;

            lineDiv.appendChild(labelDiv);
            lineDiv.appendChild(input);

            tableFilterHtmlElement.appendChild(lineDiv);
        }
        for(let filter of tableFilterHtmlElement.querySelectorAll("input")){
            filter.checked = true;
            filter.indeterminate = false;
            filter.addEventListener("change", (event) => {
                const currentFilter = event.target;
                if(currentFilter.value == "All"){
                    for(let filter of tableFilterHtmlElement.querySelectorAll("input")) {
                        filter.checked = currentFilter.checked;
                    }
                }
                else{
                    let all = true;
                    let atLeastOne = false;
                    for(let filter of tableFilterHtmlElement.querySelectorAll("input")) {
                        if(filter.checked && filter.value != "All"){
                            atLeastOne = true;
                        }
                        if(!filter.checked && filter.value != "All"){
                            all = false;
                        }
                    }
                    const allFilterElement = document.getElementById("AllFilter");
                    allFilterElement.checked = atLeastOne;
                    allFilterElement.indeterminate = (!all && atLeastOne); 
                }
                afficherListeProcessus();
            });
        }
    }
}

function parseDataToMyProcesses(data){
    if(data){
        mesProcessus = [];
        const oldCategorie = setCategorie;
        setCategorie = new Set();
        setCategorie.add("All");
        setCategorie.add("Other");
        data.forEach(process => {
            let uneApplication = new Application(process.name,process.pids, process.categorie);
            mesProcessus.push(uneApplication);
            if(uneApplication.categorie != "") {
                setCategorie.add(uneApplication.categorie);
            }
        });
        if(!areSetsEqual(oldCategorie, setCategorie)) {
            makeGroupApplication();
        }
        afficherListeProcessus();
    }
}

fetch('../Json/process.json')
.then(response => {
    // Vérifier si la réponse est correcte
    if (!response.ok) {
        throw new Error('Erreur de chargement du fichier JSON');
    }
    return response.json();  // Parse le JSON
})
.then(data => {

    // Afficher chaque processus dans la liste
    parseDataToMyProcesses(data);
})
.catch(error => {
    console.error('Erreur:', error);
});

function getFilterCategorie(nomCategorie) {
    for(let filter of tableFilterHtmlElement.querySelectorAll("input")){
        if(filter.value == nomCategorie){
            return filter.checked;
        }
    }
    const otherFilterElement = document.getElementById("OtherFilter");
    return otherFilterElement.checked;
}

function changePidState(nomProc, pidProc, etat) {
    // Adresse de votre serveur Node.js
    const serverUrl = 'http://localhost:3030/changePidState';
    // Envoi d'une requête POST au serveur
    fetch(serverUrl, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        // Ajoutez les données
        body: JSON.stringify({ nomProc, pidProc, etat }),
    })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                console.log('Script exécuté avec succès');
                //alert('Script exécuté avec succès !');
            } else {
                console.error('Erreur lors de l\'exécution :', data.message);
                alert(`Erreur : ${data.message}`);
            }
        })
        .catch(error => {
            console.error('Erreur réseau ou serveur :', error);
            alert('Impossible de contacter le serveur.');
        });
}



function afficherListeProcessus() {
    while (listeProcessusHtmlElement.firstChild) {
        listeProcessusHtmlElement.removeChild(listeProcessusHtmlElement.firstChild);
    }
    const searchText = document.getElementById("SearchBar").value == "" ? "":document.getElementById("SearchBar").value.toLowerCase();
    let atLeastOneChecked = false;
    let allChecked = true;
    for(let unProcessus of mesProcessus){
        if (!unProcessus.getName().toLowerCase().includes(searchText)){
            continue;
        }
        if(getFilterCategorie(unProcessus.categorie)){
            for(let unPid of unProcessus.getListePid()){
                // Créer les éléments
                const lineDiv = document.createElement("div");
                lineDiv.className = "line";

                const col1Div = document.createElement("div");
                col1Div.className = "col-1 cell";
                col1Div.textContent = unProcessus.getName();

                const col2Div = document.createElement("div");
                col2Div.className = "col-2 cell";

                const inputCheckbox = document.createElement("input");
                inputCheckbox.type = "checkbox";
                col2Div.textContent = unPid["numeroPid"];
                if(unPid["checked"]){
                    atLeastOneChecked = true;
                }else{
                    allChecked = false;
                }
                inputCheckbox.checked = unPid["checked"];
                inputCheckbox.setAttribute("data-nom-processus", unProcessus.getName());
                inputCheckbox.setAttribute("data-numero-pid", unPid["numeroPid"]);
                inputCheckbox.addEventListener('click', function(event) {
                    let clickedCheckbox = event.target;
                    const dataNomProcessus = event.target.getAttribute("data-nom-processus");
                    const dataNumeroPid = event.target.getAttribute("data-numero-pid");
                    changePidState(dataNomProcessus, dataNumeroPid,clickedCheckbox.checked);
                });
                col2Div.appendChild(inputCheckbox); // Ajouter la case à cocher à col-2

                // Ajouter les colonnes à la ligne
                lineDiv.appendChild(col1Div);
                lineDiv.appendChild(col2Div);

                // Ajouter la ligne à l'élément parent
                listeProcessusHtmlElement.appendChild(lineDiv);
            }
        }
    }
    checkBoxSelectAllProcElement.checked = atLeastOneChecked;
    checkBoxSelectAllProcElement.indeterminate = (!allChecked && atLeastOneChecked); 
}

function selectAllPid(etat) {
    const searchText = document.getElementById("SearchBar").value == "" ? "":document.getElementById("SearchBar").value.toLowerCase();
    let listePidAChanger = new Set();
    for(let unProcessus of mesProcessus){
        if (!unProcessus.getName().toLowerCase().includes(searchText)){
            continue;
        }
        if(!getFilterCategorie(unProcessus.categorie)){
            continue;
        }
        for(let unPid of unProcessus.getListePid()){
            if(unPid.checked != etat){
                listePidAChanger.add(unProcessus.getName());
            }
        }
    }
    console.log(listePidAChanger);
    const serverUrl = 'http://localhost:3030/changeListePidState';
    // Envoi au serveur avec fetch
    fetch(serverUrl, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json', // Assurez-vous d'envoyer du JSON
        },
        body: JSON.stringify({ liste: Array.from(listePidAChanger), etat: etat }), // Sérialise la liste dans le corps de la requête
    })
    .then(response => response.json()) // Récupère la réponse du serveur
    .then(data => {
        console.log('Réponse du serveur :', data);
    })
    .catch(error => {
        console.error('Erreur lors de l’envoi des données :', error);
    });

}

checkBoxSelectAllProcElement.addEventListener("change", (event) => {
    selectAllPid(event.target.checked);
    if (event.target.checked) {
        console.log("La case à cocher est cochée.");
    } else {
        console.log("La case à cocher est décochée.");
    }
});

const eventSource = new EventSource('http://localhost:3030/events');

eventSource.onmessage = (event) => {
    try {
        const data = JSON.parse(event.data);
        if(!data){
            console.error("Erreur data vide");
        }
        if(!data["message"]){
            parseDataToMyProcesses(data);
        }
    } catch (err) {
        console.error('Erreur de parsing des données:', err);
    }
};

eventSource.onerror = () => {
    console.error('Erreur de connexion au serveur SSE');
};

document.getElementById("SearchBar").addEventListener("keyup", () => {
    const searchText = document.getElementById("SearchBar").value;
    console.log(searchText);
    afficherListeProcessus();
});