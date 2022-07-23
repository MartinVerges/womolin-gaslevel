
export async function post({request}) {
    return {
        status: 200,
        body: {
            message: "New bottle weight set"
        }
    }
}


export async function get({request}) {
    return {
        status: 200,
        body: {
            emptyWeightGramms: Math.floor(Math.random() * 10000),
            fullWeightGramms: Math.floor(Math.random() * 10000),
        }
    }
}